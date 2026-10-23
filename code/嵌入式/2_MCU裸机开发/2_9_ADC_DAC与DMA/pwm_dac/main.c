/*
 * pwm_dac/main.c：用 PB0（TIM3_CH3）输出 20 kHz PWM，经外部 RC 低通滤波器
 * （R = 10 kOhm，C = 1 uF）产生直流模拟电压，并由 PA1（ADC1_IN1）回读验证。
 */
#include "clock.h"
#include "console.h"
#include "stm32f1xx.h"
#include "tick.h"
#include <stdint.h>
#include <stdio.h>

#define PWM_ARR 3599u

static void pwm_dac_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* PB0 配置为 50 MHz 复用推挽输出（MODE0 = 11，CNF0 = 10 -> 0xB） */
    GPIOB->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOB->CRL |= GPIO_CRL_MODE0 | GPIO_CRL_CNF0_1;

    /* 72 MHz / (0 + 1) / (3599 + 1) = 20 kHz */
    TIM3->PSC = 0u;
    TIM3->ARR = PWM_ARR;
    TIM3->CCR3 = 0u;

    /* CH3 配置为 PWM 模式 1（OC3M = 110），开启预装载（OC3PE = 1） */
    TIM3->CCMR2 &= ~(TIM_CCMR2_OC3M | TIM_CCMR2_CC3S);
    TIM3->CCMR2 |= (6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;
    TIM3->CCER |= TIM_CCER_CC3E;
    TIM3->CR1 |= TIM_CR1_ARPE;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 |= TIM_CR1_CEN;
}

static void pwm_dac_set_mv(uint32_t target_mv) {
    if (target_mv > 3300u) {
        target_mv = 3300u;
    }
    uint32_t ccr = (target_mv * (PWM_ARR + 1u) + 1650u) / 3300u;
    if (ccr > PWM_ARR) {
        ccr = PWM_ARR;
    }
    TIM3->CCR3 = (uint16_t)ccr;
}

static void adc1_init_pa1(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;
    RCC->CFGR &= ~RCC_CFGR_ADCPRE;
    RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);

    ADC1->SMPR2 &= ~ADC_SMPR2_SMP1;
    ADC1->SMPR2 |= 5u << ADC_SMPR2_SMP1_Pos;
    ADC1->SQR1 = 0u;
    ADC1->SQR3 = 1u;
    ADC1->CR2 = ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL | ADC_CR2_ADON;
    delay_ms(1);

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while ((ADC1->CR2 & ADC_CR2_RSTCAL) != 0) {
    }
    ADC1->CR2 |= ADC_CR2_CAL;
    while ((ADC1->CR2 & ADC_CR2_CAL) != 0) {
    }
}

static uint32_t adc1_read_pa1_mv(void) {
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while ((ADC1->SR & ADC_SR_EOC) == 0) {
    }
    uint16_t raw = (uint16_t)ADC1->DR;
    return ((uint32_t)raw * 3300u + 2047u) / 4095u;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);
    pwm_dac_init();
    adc1_init_pa1();

    static const uint32_t steps_mv[] = {500u, 1000u, 1650u, 2500u, 3000u};
    uint32_t idx = 0u;

    for (;;) {
        uint32_t target = steps_mv[idx];
        pwm_dac_set_mv(target);
        /* 等待 RC 滤波器稳定（约 5 * R * C = 50 ms，这里留 200 ms） */
        delay_ms(200);
        uint32_t measured = adc1_read_pa1_mv();
        printf("PWM target = %4lu mV (CCR3 = %4u), ADC readback = %4lu mV\n",
               (unsigned long)target,
               (unsigned)TIM3->CCR3,
               (unsigned long)measured);
        idx = (idx + 1u) % (sizeof steps_mv / sizeof steps_mv[0]);
        delay_ms(800);
    }
}
