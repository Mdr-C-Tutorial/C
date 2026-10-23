/*
 * tickless_stop.c：用 RTC 闹钟实现 STM32F103 的 Stop 模式无节拍空闲。
 *
 * ARM_CM3 移植层自带的 vPortSuppressTicksAndSleep() 只是把 SysTick 的重装值
 * 拉长，再执行 WFI。SysTick 只能在 Sleep 模式下把内核叫醒，所以那条路径
 * 进不了 Stop。要把 Stop 用起来，就得换一个在 Stop 下仍在工作的唤醒源：
 * RTC 闹钟经 EXTI 第 17 号线唤醒内核。
 *
 * 本文件覆盖移植层的同名弱函数，作为可编译的骨架。前提：
 *   FreeRTOSConfig.h 中 configUSE_TICKLESS_IDLE = 2；
 *   进入 Stop 前串口等外设已处理完（2.10）。
 *
 * 讲解见教程 4.7 节「低功耗」。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

/* RCC / PWR / RTC 寄存器（RM0008 第 7、8、18 章） */
#define RCC_APB1ENR (*(volatile uint32_t *)0x4002101Cu)
#define RCC_BDCR    (*(volatile uint32_t *)0x40021020u)
#define PWR_CR      (*(volatile uint32_t *)0x40007000u)
#define PWR_CSR     (*(volatile uint32_t *)0x40007004u)
#define RTC_CRL     (*(volatile uint32_t *)0x40002804u)
#define RTC_PRLL    (*(volatile uint32_t *)0x4000280Cu)
#define RTC_PRHH    (*(volatile uint32_t *)0x40002810u)
#define RTC_CNTL    (*(volatile uint32_t *)0x40002818u)
#define RTC_CNTH    (*(volatile uint32_t *)0x4000281Cu)
#define RTC_ALRL    (*(volatile uint32_t *)0x40002820u)
#define RTC_ALRH    (*(volatile uint32_t *)0x40002824u)

/* SysTick（PM0056 第 4.5 节） */
#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)

#define RCC_APB1ENR_PWREN   (1u << 28)
#define RCC_APB1ENR_BKPEN   (1u << 27)
#define RCC_BDCR_LSEON      (1u << 0)
#define RCC_BDCR_LSERDY     (1u << 1)
#define RCC_BDCR_RTCSEL_LSE (1u << 8)
#define RCC_BDCR_RTCEN      (1u << 15)
#define RCC_BDCR_BDRST      (1u << 16)
#define PWR_CR_LPDS         (1u << 0)
#define PWR_CR_PDDS         (1u << 1)
#define PWR_CR_CWUF         (1u << 2)
#define RTC_CRL_SECF        (1u << 0)
#define RTC_CRL_ALRF        (1u << 1)
#define RTC_CRL_RSF         (1u << 3)
#define RTC_CRL_CNF         (1u << 4)
#define RTC_CRL_RTOFF       (1u << 5)

/* LSE 为 32768 Hz；预分频写 32767，RTC 计数器每秒加一。 */
#define RTC_CLOCK_HZ  32768u
#define RTC_PRESCALER (RTC_CLOCK_HZ - 1u)

extern void clock_init_72mhz(void);

static uint32_t rtc_read_counter(void) {
    uint32_t high1;
    uint32_t high2;
    uint32_t low;

    do {
        high1 = RTC_CNTH;
        low = RTC_CNTL;
        high2 = RTC_CNTH;
    } while (high1 != high2);

    return (high1 << 16) | (low & 0xFFFFu);
}

static void rtc_wait_rtoff(void) {
    while ((RTC_CRL & RTC_CRL_RTOFF) == 0) {
    }
}

static void rtc_set_alarm(uint32_t value) {
    rtc_wait_rtoff();
    RTC_CRL |= RTC_CRL_CNF;
    RTC_ALRH = value >> 16;
    RTC_ALRL = value & 0xFFFFu;
    RTC_CRL &= ~RTC_CRL_CNF;
    rtc_wait_rtoff();
    RTC_CRL &= ~RTC_CRL_ALRF;
}

static void rtc_init_lse(void) {
    RCC_APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR_CR |= PWR_CR_CWUF;

    /* 复位备份域，让 RTC 从已知状态开始；复位会清掉 BDCR，所以随后重配 */
    RCC_BDCR |= RCC_BDCR_BDRST;
    RCC_BDCR &= ~RCC_BDCR_BDRST;

    RCC_BDCR |= RCC_BDCR_LSEON;
    while ((RCC_BDCR & RCC_BDCR_LSERDY) == 0) {
    }

    RCC_BDCR = (RCC_BDCR & ~(3u << 8)) | RCC_BDCR_RTCSEL_LSE;
    RCC_BDCR |= RCC_BDCR_RTCEN;

    rtc_wait_rtoff();
    RTC_CRL |= RTC_CRL_CNF;
    RTC_PRLL = RTC_PRESCALER & 0xFFFFu;
    RTC_PRHH = RTC_PRESCALER >> 16;
    RTC_CRL &= ~RTC_CRL_CNF;
    rtc_wait_rtoff();

    RTC_CRL &= ~(RTC_CRL_ALRF | RTC_CRL_SECF);
}

/* 进入 Stop，直到 RTC 闹钟（EXTI 17）或其他使能的中断把内核叫醒 */
static void enter_stop(void) {
    PWR_CR |= PWR_CR_LPDS;
    PWR_CR &= ~PWR_CR_PDDS;
    PWR_CR |= PWR_CR_CWUF;
    __asm volatile("dsb");
    __asm volatile("wfi");
    __asm volatile("isb");
}

void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime) {
    static int s_rtc_ready;
    uint32_t sleep_seconds;
    uint32_t start;
    uint32_t elapsed;
    TickType_t tick_before;

    if (s_rtc_ready == 0) {
        rtc_init_lse();
        s_rtc_ready = 1;
    }

    /* RTC 的闹钟分辨率为 1 秒。不足两秒的睡眠不划算，直接交回默认路径。 */
    sleep_seconds = (uint32_t)xExpectedIdleTime / (uint32_t)configTICK_RATE_HZ;
    if (sleep_seconds < 2u) {
        return;
    }

    /* 关中断：从这里到 WFI 之间不允许切换任务 */
    __asm volatile("cpsid i" ::: "memory");
    __asm volatile("dsb");
    __asm volatile("isb");

    if (eTaskConfirmSleepModeStatus() == eAbortSleep) {
        __asm volatile("cpsie i" ::: "memory");
        return;
    }

    /* 停 SysTick，记下当前的节拍数，准备睡整秒 */
    SYST_CSR = 0;
    tick_before = xTaskGetTickCount();
    start = rtc_read_counter();
    rtc_set_alarm(start + sleep_seconds);

    /* 放行中断，让 RTC 闹钟能唤醒；WFI 等待事件 */
    __asm volatile("cpsie i" ::: "memory");
    enter_stop();

    /* 醒来：先恢复时钟，再按实际睡过的秒数补节拍 */
    clock_init_72mhz();
    elapsed = rtc_read_counter() - start;
    vTaskStepTick((TickType_t)elapsed * configTICK_RATE_HZ);
    (void)tick_before;

    /* 重设 SysTick：一个节拍的重装值，从零开始计 */
    SYST_RVR = (configCPU_CLOCK_HZ / configTICK_RATE_HZ) - 1UL;
    SYST_CVR = 0;
    SYST_CSR = 0x7u;
}
