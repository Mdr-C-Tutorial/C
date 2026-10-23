/*
 * adc_timer_trigger/main.c：TIM3 更新事件（TRGO）以 1 kHz 定时触发 ADC1 采样，
 * DMA1 通道 1 采集满 64 个点后由主循环计算平均值、最小值和最大值。
 */
#include "clock.h"
#include "console.h"
#include "stm32f1xx.h"
#include "tick.h"
#include <stdint.h>
#include <stdio.h>

#define SAMPLE_COUNT 64u

static volatile uint16_t g_samples[SAMPLE_COUNT];

static void tim3_trigger_init_1khz(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* TIM3 时钟为 72 MHz：PSC = 71（1 MHz 计数），ARR = 999 -> 更新频率 1 kHz */
    TIM3->PSC = 71u;
    TIM3->ARR = 999u;

    /* MMS = 010（Update）：每次计数器溢出更新时在 TRGO 输出触发脉冲 */
    TIM3->CR2 &= ~TIM_CR2_MMS;
    TIM3->CR2 |= TIM_CR2_MMS_1;
    TIM3->EGR = TIM_EGR_UG;
}

static void adc1_tim3_dma_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    RCC->CFGR &= ~RCC_CFGR_ADCPRE;
    RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;

    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);

    ADC1->SMPR2 &= ~ADC_SMPR2_SMP1;
    ADC1->SMPR2 |= 5u << ADC_SMPR2_SMP1_Pos;
    ADC1->SQR1 = 0u;
    ADC1->SQR3 = 1u;

    /*
     * EXTSEL = 100：选择 TIM3_TRGO 作为规则组触发源；
     * CONT = 0：不开启连续模式，每次 TRGO 脉冲只转换一次。
     */
    ADC1->CR2 = ADC_CR2_EXTTRIG |
                ADC_CR2_EXTSEL_2 |
                ADC_CR2_DMA |
                ADC_CR2_ADON;
    delay_ms(1);

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while ((ADC1->CR2 & ADC_CR2_RSTCAL) != 0) {
    }
    ADC1->CR2 |= ADC_CR2_CAL;
    while ((ADC1->CR2 & ADC_CR2_CAL) != 0) {
    }
}

static void capture_64_samples(void) {
    TIM3->CR1 &= ~TIM_CR1_CEN;

    DMA1_Channel1->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    DMA1_Channel1->CPAR = (uint32_t)(uintptr_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)(uintptr_t)g_samples;
    DMA1_Channel1->CNDTR = SAMPLE_COUNT;
    DMA1_Channel1->CCR = DMA_CCR_PSIZE_0 |
                         DMA_CCR_MSIZE_0 |
                         DMA_CCR_MINC |
                         DMA_CCR_EN;

    /* 启动 TIM3，开始以 1 kHz 产生 TRGO 触发脉冲 */
    TIM3->CNT = 0u;
    TIM3->CR1 |= TIM_CR1_CEN;

    while ((DMA1->ISR & DMA_ISR_TCIF1) == 0) {
    }
    TIM3->CR1 &= ~TIM_CR1_CEN;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);
    tim3_trigger_init_1khz();
    adc1_tim3_dma_init();

    for (;;) {
        capture_64_samples();

        uint32_t sum = 0u;
        uint16_t min_val = 4095u;
        uint16_t max_val = 0u;
        for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i) {
            uint16_t v = g_samples[i];
            sum += v;
            if (v < min_val) {
                min_val = v;
            }
            if (v > max_val) {
                max_val = v;
            }
        }
        uint32_t avg = (sum + (SAMPLE_COUNT / 2u)) / SAMPLE_COUNT;
        printf("64 samples @ 1 kHz: avg=%4lu, min=%4u, max=%4u\n",
               (unsigned long)avg, (unsigned)min_val, (unsigned)max_val);
        delay_ms(1000);
    }
}
