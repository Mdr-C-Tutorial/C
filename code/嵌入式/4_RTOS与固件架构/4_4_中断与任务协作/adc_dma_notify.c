/*
 * adc_dma_notify.c：ADC1 的 DMA 传输完成中断 → vTaskNotifyGiveFromISR → 处理任务。
 *
 * 讲解见教程 4.4 节「中断与任务协作」。
 *
 * ADC1 由 TIM2 的 TRGO 以 1 kHz 触发，DMA1 通道 1 把每次转换结果搬进两个
 * 16 位缓冲区（半满 / 全满各算一批）。中断里只通知任务，算术平均在任务里做。
 */
#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "stm32f1xx.h"

#define ADC_SAMPLES        32u
#define ADC_IRQ_PRIO       5
#define ADC_NOTIFY_ON_HALF 1

static volatile uint16_t g_adc_buf[ADC_SAMPLES];
static TaskHandle_t g_adc_task;
static volatile uint32_t g_batch_count;
static volatile uint8_t g_half_pending;
static volatile uint8_t g_full_pending;

void DMA1_Channel1_IRQHandler(void) {
    BaseType_t higher_woken = pdFALSE;
    uint32_t isr = DMA1->ISR;

    if ((isr & DMA_ISR_HTIF1) != 0u) {
        DMA1->IFCR = DMA_IFCR_CHTIF1 | DMA_IFCR_CGIF1;
        g_half_pending = 1;
        vTaskNotifyGiveFromISR(g_adc_task, &higher_woken);
    }
    if ((isr & DMA_ISR_TCIF1) != 0u) {
        DMA1->IFCR = DMA_IFCR_CTCIF1 | DMA_IFCR_CGIF1;
        g_full_pending = 1;
        vTaskNotifyGiveFromISR(g_adc_task, &higher_woken);
    }

    portYIELD_FROM_ISR(higher_woken);
}

static void adc_dma_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    /* PA1 模拟输入（ADC12_IN1，见 2.9 节） */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 4)) | (0x0u << 4);

    /* ADC1 时钟 12 MHz（PCLK2 72 MHz ÷ 6），规则通道 1，采样时间 55.5 个周期 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | RCC_CFGR_ADCPRE_DIV6;
    ADC1->SMPR2 |= ADC_SMPR2_SMP1;
    ADC1->SQR1 = 0u;
    ADC1->SQR3 = 1u;
    ADC1->CR2 = ADC_CR2_DMA | ADC_CR2_CONT;

    DMA1_Channel1->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    DMA1_Channel1->CPAR = (uint32_t)(uintptr_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)(uintptr_t)g_adc_buf;
    DMA1_Channel1->CNDTR = ADC_SAMPLES;
    DMA1_Channel1->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PSIZE_0 |
                         DMA_CCR_MSIZE_0 | DMA_CCR_HTIE | DMA_CCR_TCIE |
                         DMA_CCR_EN;

    NVIC_SetPriority(DMA1_Channel1_IRQn, ADC_IRQ_PRIO);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    ADC1->CR2 |= ADC_CR2_ADON;
    for (volatile uint32_t i = 0; i < 1000u; ++i) {
    }
    ADC1->CR2 |= ADC_CR2_CAL;
    while ((ADC1->CR2 & ADC_CR2_CAL) != 0u) {
    }
}

static void adc_process_task(void *param) {
    (void)param;

    adc_dma_init();
    ADC1->CR2 |= ADC_CR2_SWSTART;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        g_batch_count++;

        if (g_half_pending != 0u) {
            g_half_pending = 0;
        }
        if (g_full_pending != 0u) {
            g_full_pending = 0;
        }
        /* 平均值计算与滤波在 4.3 的 sample_t 流程里做过，这里省略 */
    }
}

int main(void) {
    if (xTaskCreate(adc_process_task, "adc", 256, NULL, 3, &g_adc_task) != pdPASS) {
        return 1;
    }
    vTaskStartScheduler();
    for (;;) {
    }
}