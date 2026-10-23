/*
 * adc_single/main.c：ADC1 单次转换读取 PA1（ADC12_IN1）电位器电压。
 *
 * 硬件连接：
 *   - 电位器两端接 3.3 V 与 GND，滑动端接 PA1
 *   - PA9（USART1_TX）接 USB 转串口 RXD，115200 8N1
 */
#include "clock.h"
#include "console.h"
#include "stm32f1xx.h"
#include "tick.h"
#include <stdint.h>
#include <stdio.h>

static void adc1_init_single(void) {
    /* 1. 开启 GPIOA 与 ADC1 时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;

    /* 2. 配置 ADC 预分频器：PCLK2 = 72 MHz，6 分频后 ADCCLK = 12 MHz（<= 14 MHz） */
    RCC->CFGR &= ~RCC_CFGR_ADCPRE;
    RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;

    /* 3. 把 PA1 配置为模拟输入（MODE1 = 00，CNF1 = 00） */
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);

    /* 4. 配置通道 1 采样时间为 55.5 个 ADC 周期（SMP1 = 101） */
    ADC1->SMPR2 &= ~ADC_SMPR2_SMP1;
    ADC1->SMPR2 |= 5u << ADC_SMPR2_SMP1_Pos;

    /* 5. 规则序列长度 L = 0（1 个转换），第 1 个转换 SQ1 = 通道 1 */
    ADC1->SQR1 &= ~ADC_SQR1_L;
    ADC1->SQR3 = 1u;

    /* 6. 规则组选择软件触发（EXTSEL = 111）并使能外部触发（EXTTRIG = 1） */
    ADC1->CR2 &= ~ADC_CR2_EXTSEL;
    ADC1->CR2 |= ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG;

    /* 7. 首次置位 ADON，唤醒 ADC，等待至少 tSTAB（1 us）稳定 */
    ADC1->CR2 |= ADC_CR2_ADON;
    delay_ms(1);

    /* 8. 复位校准寄存器并执行自校准，等待硬件清零 */
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while ((ADC1->CR2 & ADC_CR2_RSTCAL) != 0) {
    }
    ADC1->CR2 |= ADC_CR2_CAL;
    while ((ADC1->CR2 & ADC_CR2_CAL) != 0) {
    }
}

static uint16_t adc1_read_once(void) {
    /* 启动规则组转换 */
    ADC1->CR2 |= ADC_CR2_SWSTART;

    /* 等待转换结束标志 EOC 置位 */
    while ((ADC1->SR & ADC_SR_EOC) == 0) {
    }

    /* 读取 DR 的同时硬件自动清除 EOC */
    return (uint16_t)ADC1->DR;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);
    adc1_init_single();

    for (;;) {
        uint16_t raw = adc1_read_once();
        uint32_t mv = ((uint32_t)raw * 3300u + 2047u) / 4095u;
        printf("PA1 raw = %4u, voltage = %4lu mV\n",
               (unsigned)raw, (unsigned long)mv);
        delay_ms(500);
    }
}
