/*
 * isr_notify_sim.c：模拟 ADC 的 DMA 传输完成中断，用任务通知把它交给处理任务。
 *
 * 讲解见教程 4.4 节「中断与任务协作」。
 *
 * PC 模拟器上没有真的 ADC，用一个低优先级任务充当“外设”，每隔 20 ms
 * 调用 vPortGenerateSimulatedInterrupt() 触发一次模拟中断。中断处理函数
 * 以返回值代替 portYIELD_FROM_ISR：返回非零表示需要切换任务。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "rtos_console.h"

#define ADC_IRQ_NUMBER 2u

typedef struct {
    TickType_t tick;
    int16_t ax;
    int16_t ay;
    int16_t az;
} sample_t;

/* 中断与处理任务之间只传“一个样本 + 一个计数”，用单槽缓冲即可 */
static volatile sample_t g_isr_sample;
static volatile uint32_t g_isr_count;
static TaskHandle_t g_processor_handle;

/* 中断处理函数：模拟 ADC 的 DMA 传输完成。运行在模拟中断线程里 */
static uint32_t adc_dma_complete_isr(void) {
    BaseType_t higher_woken = pdFALSE;

    static const int16_t table[][3] = {
        { 12, -18, 998 },
        { 16, -14, 1006 },
        { 10, -20, 1002 },
        { 14, -16, 994 },
    };
    uint32_t index = g_isr_count % 4u;

    /* 中断里只做必须马上做的事：取出数据、记下时刻、通知任务 */
    g_isr_sample.tick = xTaskGetTickCountFromISR();
    g_isr_sample.ax = table[index][0];
    g_isr_sample.ay = table[index][1];
    g_isr_sample.az = table[index][2];
    g_isr_count++;

    vTaskNotifyGiveFromISR(g_processor_handle, &higher_woken);

#ifdef NO_YIELD_FROM_ISR
    /* 反例：丢掉了 higher_woken，被唤醒的任务要等到下一个节拍才运行 */
    (void)higher_woken;
    return 0;
#else
    /* 唤醒的任务优先级更高时返回 pdTRUE，退出中断时立刻切换 */
    return (uint32_t)higher_woken;
#endif
}

static void adc_poll_task(void *param) {
    (void)param;

    for (uint32_t i = 0; i < 4u; ++i) {
        vTaskDelay(pdMS_TO_TICKS(20));
        console_printf("[adc ] 触发第 %u 次模拟中断\n", (unsigned)(i + 1u));
        vPortGenerateSimulatedInterrupt(ADC_IRQ_NUMBER);
        console_printf("[adc ] 中断返回后继续执行（本任务优先级 1）\n");
    }

    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void processor_task(void *param) {
    (void)param;

    for (uint32_t i = 0; i < 4u; ++i) {
        /* 第一个参数 pdFALSE：每取走一次通知计数减 1 */
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

        sample_t s = {
            .tick = g_isr_sample.tick,
            .ax = g_isr_sample.ax,
            .ay = g_isr_sample.ay,
            .az = g_isr_sample.az,
        };
        console_printf("[proc] 被中断唤醒（优先级 3），tick=%lu 样本 ax=%d ay=%d az=%d\n",
                       (unsigned long)s.tick, (int)s.ax, (int)s.ay, (int)s.az);
    }

    sim_finish();
}

int main(void) {
    vPortSetInterruptHandler(ADC_IRQ_NUMBER, adc_dma_complete_isr);

    if (xTaskCreate(processor_task, "proc", 256, NULL, 3, &g_processor_handle) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(adc_poll_task, "adc", 256, NULL, 1, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}