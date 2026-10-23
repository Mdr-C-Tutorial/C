/*
 * dwt_demo/main.c：用 DWT 的周期计数器测量一段代码的耗时，并实现微秒级延时。
 *
 * DWT 是 Cortex-M3 内核里的调试组件，CYCCNT 每个内核周期加一。
 * 72 MHz 下 1 us 对应 72 个周期。
 */
#include <stdint.h>
#include "stm32f1xx.h"
#include "clock.h"

void dwt_init(void) {
    /* 使能 DWT 与 ITM 的总开关 TRCENA，再启动周期计数器 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000u);
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
    }
}

static void led_pc13_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xFu << 20);
    GPIOC->CRH |= 0x2u << 20;
    GPIOC->BSRR = GPIO_BSRR_BS13;
}

/* 调试器里查看这两个对象：10 us 延时的真实周期数、1000 次翻转的周期数 */
volatile uint32_t g_ten_us_cycles;
volatile uint32_t g_toggle_cycles;

int main(void) {
    clock_init_72mhz();
    led_pc13_init();
    dwt_init();

    uint32_t t0 = DWT->CYCCNT;
    delay_us(10u);
    g_ten_us_cycles = DWT->CYCCNT - t0;

    t0 = DWT->CYCCNT;
    for (int i = 0; i < 1000; ++i) {
        GPIOC->BSRR = GPIO_BSRR_BR13;
        GPIOC->BSRR = GPIO_BSRR_BS13;
    }
    g_toggle_cycles = DWT->CYCCNT - t0;

    for (;;) {
    }
}