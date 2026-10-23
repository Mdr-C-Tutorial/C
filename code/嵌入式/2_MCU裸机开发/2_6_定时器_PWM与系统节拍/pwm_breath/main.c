/*
 * pwm_breath/main.c：TIM3_CH3（PB0）输出 1 kHz PWM 实现呼吸灯。
 *
 * 参数计算：
 *   f_TIM = 72 MHz
 *   PSC = 71  -> f_CNT = 72 MHz / 72 = 1 MHz（1 us/步）
 *   ARR = 999 -> PWM 频率 = 1 MHz / 1000 = 1 kHz
 *   CCR3 取 0..1000 对应占空比 0%..100%
 */
#include <stdint.h>
#include "stm32f1xx.h"
#include "clock.h"
#include "../tick.h"

static void tim3_pwm_pb0_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* PB0（TIM3_CH3）：复用推挽输出，最大速度 2 MHz（CNF0 = 10，MODE0 = 10） */
    GPIOB->CRL &= ~(0xFu << 0);
    GPIOB->CRL |= 0xAu << 0;

    TIM3->PSC = 71u;
    TIM3->ARR = 999u;
    TIM3->CCR3 = 0u;

    /*
     * 通道 3 配置在 CCMR2 低 8 位：
     *   CC3S[1:0] = 00（输出）
     *   OC3PE     = 1 （开启 CCR3 预装载）
     *   OC3M[2:0] = 110（PWM 模式 1）
     */
    TIM3->CCMR2 &= ~(TIM_CCMR2_CC3S_Msk | TIM_CCMR2_OC3M_Msk);
    TIM3->CCMR2 |= (0x6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;

    /* 使能通道 3 输出，高电平有效（CC3P = 0，CC3E = 1） */
    TIM3->CCER &= ~TIM_CCER_CC3P;
    TIM3->CCER |= TIM_CCER_CC3E;

    TIM3->CR1 |= TIM_CR1_ARPE;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = (uint16_t)~TIM_SR_UIF;
    TIM3->CR1 |= TIM_CR1_CEN;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    tim3_pwm_pb0_init();

    uint32_t last_step_ms = tick_now();
    int32_t duty = 0;
    int32_t delta = 10;

    for (;;) {
        uint32_t now = tick_now();
        if ((uint32_t)(now - last_step_ms) >= 10u) {
            last_step_ms = now;
            duty += delta;
            if (duty >= 1000) {
                duty = 1000;
                delta = -10;
            } else if (duty <= 0) {
                duty = 0;
                delta = 10;
            }
            TIM3->CCR3 = (uint16_t)duty;
        }
    }
}
