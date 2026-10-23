/*
 * tim_irq/main.c：使用通用定时器 TIM2 产生 1 Hz 更新中断翻转 PC13 LED。
 *
 * 时钟计算：
 *   PCLK1 = 36 MHz，APB1 预分频系数为 2（不为 1），因此 TIM2 时钟 f_TIM = 72 MHz。
 *   PSC = 7199 -> 计数器时钟 f_CNT = 72 MHz / 7200 = 10 kHz（每步 0.1 ms）
 *   ARR = 9999 -> 更新事件频率 f_UEV = 10 kHz / 10000 = 1 Hz
 */
#include <stdint.h>
#include "stm32f1xx.h"
#include "clock.h"

static void gpio_pc13_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xFu << 20);
    GPIOC->CRH |= 0x2u << 20;
    GPIOC->BSRR = GPIO_BSRR_BS13;
}

static void tim2_init_1hz(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->PSC = 7199u;
    TIM2->ARR = 9999u;
    TIM2->CR1 |= TIM_CR1_ARPE;

    /* 产生一次更新事件，使 PSC 与 ARR 立即装入影子寄存器 */
    TIM2->EGR = TIM_EGR_UG;
    /* 清除 UG 置位的 UIF 标志（rc_w0 位：写 0 清零，写 1 无影响） */
    TIM2->SR = (uint16_t)~TIM_SR_UIF;

    TIM2->DIER |= TIM_DIER_UIE;
    NVIC_SetPriority(TIM2_IRQn, 2u);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void) {
    if ((TIM2->SR & TIM_SR_UIF) != 0u) {
        TIM2->SR = (uint16_t)~TIM_SR_UIF;
        if ((GPIOC->ODR & GPIO_ODR_ODR13) != 0u) {
            GPIOC->BSRR = GPIO_BSRR_BR13;
        } else {
            GPIOC->BSRR = GPIO_BSRR_BS13;
        }
    }
}

int main(void) {
    clock_init_72mhz();
    gpio_pc13_init();
    tim2_init_1hz();

    for (;;) {
        __WFI();
    }
}
