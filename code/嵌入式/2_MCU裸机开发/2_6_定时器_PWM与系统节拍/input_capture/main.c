/*
 * input_capture/main.c：用 TIM2_CH1（PA0）输入捕获测量 PB0（TIM3_CH3）输出的 PWM 周期。
 *
 * 硬件接线：用杜邦线把 PB0 与 PA0 短接。
 *   - TIM3_CH3（PB0）输出 1 kHz、占空比 25% 的测试方波；
 *   - TIM2_CH1（PA0）以 1 MHz（1 us/步）自由计数，在每个上升沿锁存 CNT 到 CCR1；
 *   - 相邻两次捕获值按 uint16_t 无符号相减，即使 CNT 发生 16 位回绕也能算出正确周期。
 */
#include <stdint.h>
#include "stm32f1xx.h"
#include "clock.h"

volatile uint16_t g_period_us = 0;
volatile uint32_t g_freq_hz = 0;

static uint16_t s_last_cap = 0;
static uint8_t s_has_first = 0;

static void tim3_pwm_1khz_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    GPIOB->CRL &= ~(0xFu << 0);
    GPIOB->CRL |= 0xAu << 0;

    TIM3->PSC = 71u;
    TIM3->ARR = 999u;
    TIM3->CCR3 = 250u; /* 25% 占空比 */

    TIM3->CCMR2 &= ~(TIM_CCMR2_CC3S_Msk | TIM_CCMR2_OC3M_Msk);
    TIM3->CCMR2 |= (0x6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;
    TIM3->CCER |= TIM_CCER_CC3E;
    TIM3->CR1 |= TIM_CR1_ARPE;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = (uint16_t)~TIM_SR_UIF;
    TIM3->CR1 |= TIM_CR1_CEN;
}

static void tim2_ic_pa0_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* PA0（TIM2_CH1）：浮空输入（CNF0 = 01，MODE0 = 00） */
    GPIOA->CRL &= ~(0xFu << 0);
    GPIOA->CRL |= 0x4u << 0;

    /* 1 MHz 计数（1 us/步），ARR = 0xFFFF 满量程自由运行 */
    TIM2->PSC = 71u;
    TIM2->ARR = 0xFFFFu;

    /* CC1S = 01：IC1 映射到 TI1（PA0），无滤波、无预分频 */
    TIM2->CCMR1 &= ~(TIM_CCMR1_CC1S_Msk | TIM_CCMR1_IC1F_Msk | TIM_CCMR1_IC1PSC_Msk);
    TIM2->CCMR1 |= 0x1u << TIM_CCMR1_CC1S_Pos;

    /* CC1P = 0（上升沿捕获），CC1E = 1（使能捕获） */
    TIM2->CCER &= ~TIM_CCER_CC1P;
    TIM2->CCER |= TIM_CCER_CC1E;

    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    TIM2->DIER |= TIM_DIER_CC1IE;

    NVIC_SetPriority(TIM2_IRQn, 1u);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void) {
    if ((TIM2->SR & TIM_SR_CC1IF) != 0u) {
        /* 读取 CCR1 会由硬件自动清除 CC1IF */
        uint16_t cap = (uint16_t)TIM2->CCR1;
        if (s_has_first != 0u) {
            uint16_t diff = (uint16_t)(cap - s_last_cap);
            g_period_us = diff;
            if (diff != 0u) {
                g_freq_hz = 1000000u / diff;
            }
        } else {
            s_has_first = 1u;
        }
        s_last_cap = cap;
    }
}

int main(void) {
    clock_init_72mhz();
    tim3_pwm_1khz_init();
    tim2_ic_pa0_init();

    for (;;) {
        __WFI();
    }
}
