/*
 * main.c：2.5 节按键外部中断示例（PA0 上升沿触发 EXTI0，翻转 PC13 板载 LED）。
 *
 * 硬件连接：
 *   - PA0：按键一端接 3.3 V，配置为内部下拉输入，按下时产生上升沿
 *   - PC13：板载 LED（低电平点亮），配置为 2 MHz 通用推挽输出
 */
#include <stdint.h>
#include "stm32f1xx.h"
#include "clock.h"

/* 主循环与中断服务程序共享的静态存储期对象 */
static volatile uint32_t g_press_count;
static volatile uint32_t g_button_flag;

/*
 * EXTI0 中断服务程序：函数名必须与启动文件向量表中的弱符号完全一致。
 * 进入时硬件已自动保存 R0～R3、R12、LR、PC、xPSR，LR 中存放 EXC_RETURN。
 */
void EXTI0_IRQHandler(void) {
    if ((EXTI->PR & EXTI_PR_PR0) != 0u) {
        /* 写 1 清除 EXTI0 挂起位，尽早清除以免退出后误重入 */
        EXTI->PR = EXTI_PR_PR0;

        /* 用 BSRR 翻转 PC13 输出电平 */
        if ((GPIOC->ODR & GPIO_ODR_ODR13) != 0u) {
            GPIOC->BSRR = GPIO_BSRR_BR13;
        } else {
            GPIOC->BSRR = GPIO_BSRR_BS13;
        }

        g_press_count = g_press_count + 1u;
        g_button_flag = 1u;
    }
}

/*
 * 在临界区内原子地取出并清零累计按键次数，随后恢复进入前的 PRIMASK 状态。
 */
uint32_t take_press_count(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t count = g_press_count;
    g_press_count = 0u;
    __set_PRIMASK(primask);
    return count;
}

static void exti0_button_init(void) {
    /* 1. 开启 GPIOA、GPIOC 与 AFIO 的 APB2 时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;

    /* 2. PC13 配置为 2 MHz 通用推挽输出（CNF13 = 00，MODE13 = 10），初始熄灭 LED */
    GPIOC->CRH &= ~(0xFu << 20);
    GPIOC->CRH |= (0x2u << 20);
    GPIOC->BSRR = GPIO_BSRR_BS13;

    /* 3. PA0 配置为上拉/下拉输入（CNF0 = 10，MODE0 = 00），ODR0 = 0 选择内部下拉 */
    GPIOA->CRL &= ~(0xFu << 0);
    GPIOA->CRL |= (0x8u << 0);
    GPIOA->BSRR = GPIO_BSRR_BR0;

    /* 4. 通过 AFIO_EXTICR1 将 EXTI0 连接到 PA0（EXTI0[3:0] = 0000） */
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;

    /* 5. 配置 EXTI0 为上升沿触发，禁止下降沿触发 */
    EXTI->RTSR |= EXTI_RTSR_TR0;
    EXTI->FTSR &= ~EXTI_FTSR_TR0;

    /* 6. 使能中断前先写 1 清除可能残留的挂起标志，再开放 EXTI0 中断屏蔽位 */
    EXTI->PR = EXTI_PR_PR0;
    EXTI->IMR |= EXTI_IMR_MR0;

    /* 7. 4 位优先级全部用作抢占优先级（分组 3），设 EXTI0 优先级为 2 并使能 */
    NVIC_SetPriorityGrouping(3u);
    NVIC_SetPriority(EXTI0_IRQn, 2u);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

int main(void) {
    clock_init_72mhz();
    exti0_button_init();

    uint32_t total_handled = 0u;
    for (;;) {
        if (g_button_flag != 0u) {
            g_button_flag = 0u;
            total_handled += take_press_count();
            (void)total_handled;
        }
    }
}
