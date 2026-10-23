/*
 * adc_scan_dma/main.c：ADC1 扫描转换（IN1、IN16、IN17）+ DMA1 通道 1 循环搬运。
 *
 * 三个通道依次为：
 *   - adc_buf[0]：IN1（PA1 电位器）
 *   - adc_buf[1]：IN16（片内温度传感器）
 *   - adc_buf[2]：IN17（内部参考电压 VREFINT，典型值 1200 mV）
 */
#include "clock.h"
#include "console.h"
#include "stm32f1xx.h"
#include "tick.h"
#include <stdint.h>
#include <stdio.h>

#define ADC_CH_COUNT 3u

static volatile uint16_t g_adc_buf[ADC_CH_COUNT];

static void adc1_scan_dma_init(void) {
    /* 1. 开启 GPIOA、ADC1 与 DMA1 时钟，设置 ADCCLK = 72 MHz / 6 = 12 MHz */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    RCC->CFGR &= ~RCC_CFGR_ADCPRE;
    RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;

    /* 2. PA1 设为模拟输入 */
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);

    /* 3. 配置 DMA1 通道 1：外设 &ADC1->DR -> 内存 g_adc_buf，16 位，循环 + 内存递增 */
    DMA1_Channel1->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    DMA1_Channel1->CPAR = (uint32_t)(uintptr_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)(uintptr_t)g_adc_buf;
    DMA1_Channel1->CNDTR = ADC_CH_COUNT;
    DMA1_Channel1->CCR = DMA_CCR_PSIZE_0 |
                         DMA_CCR_MSIZE_0 |
                         DMA_CCR_MINC |
                         DMA_CCR_CIRC |
                         DMA_CCR_EN;

    /*
     * 4. 采样时间：
     *    - IN1：55.5 周期（SMP1 = 101）
     *    - IN16（温度）与 IN17（VREFINT）：手册要求 >= 17.1 us，
     *      12 MHz 下选 239.5 周期（SMP = 111，约 20.0 us）
     */
    ADC1->SMPR2 &= ~ADC_SMPR2_SMP1;
    ADC1->SMPR2 |= 5u << ADC_SMPR2_SMP1_Pos;
    ADC1->SMPR1 |= ADC_SMPR1_SMP16 | ADC_SMPR1_SMP17;

    /* 5. 规则序列：长度 3（L = 2），依次转换 IN1 -> IN16 -> IN17 */
    ADC1->SQR1 = (ADC_CH_COUNT - 1u) << ADC_SQR1_L_Pos;
    ADC1->SQR3 = (1u << 0) | (16u << 5) | (17u << 10);

    /* 6. 开启扫描模式（SCAN = 1） */
    ADC1->CR1 = ADC_CR1_SCAN;

    /* 7. 开启连续转换、DMA 请求、内部温度/参考电压通道与软件触发，并上电 */
    ADC1->CR2 = ADC_CR2_TSVREFE |
                ADC_CR2_EXTTRIG |
                ADC_CR2_EXTSEL |
                ADC_CR2_DMA |
                ADC_CR2_CONT |
                ADC_CR2_ADON;
    delay_ms(1);

    /* 8. 复位校准并执行校准 */
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while ((ADC1->CR2 & ADC_CR2_RSTCAL) != 0) {
    }
    ADC1->CR2 |= ADC_CR2_CAL;
    while ((ADC1->CR2 & ADC_CR2_CAL) != 0) {
    }

    /* 9. 启动连续扫描转换 */
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);
    adc1_scan_dma_init();

    for (;;) {
        uint16_t raw_pa1 = g_adc_buf[0];
        uint16_t raw_temp = g_adc_buf[1];
        uint16_t raw_vref = g_adc_buf[2];

        if (raw_vref != 0u) {
            uint32_t vdda_mv = (1200u * 4095u + (raw_vref / 2u)) / raw_vref;
            uint32_t pa1_mv = ((uint32_t)raw_pa1 * vdda_mv + 2047u) / 4095u;
            uint32_t vsense_mv = ((uint32_t)raw_temp * vdda_mv + 2047u) / 4095u;
            int32_t temp_decic =
                ((1430 - (int32_t)vsense_mv) * 100) / 43 + 250;

            printf("VDDA=%4lu mV | PA1=%4lu mV (raw=%4u) | T=%ld.%ld C\n",
                   (unsigned long)vdda_mv,
                   (unsigned long)pa1_mv,
                   (unsigned)raw_pa1,
                   (long)(temp_decic / 10),
                   (long)(temp_decic >= 0 ? temp_decic % 10 : -(temp_decic % 10)));
        }
        delay_ms(500);
    }
}
