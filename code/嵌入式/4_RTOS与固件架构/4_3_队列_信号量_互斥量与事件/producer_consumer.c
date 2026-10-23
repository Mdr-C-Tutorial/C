/*
 * producer_consumer.c：用队列在采样任务与处理任务之间传递 sample_t 数据。
 *
 * 讲解见教程 4.3 节「队列、信号量、互斥量与事件」。
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "sim_support.h"

#define SAMPLE_COUNT 6u

static QueueHandle_t g_sample_queue;

static void sampler_task(void *param) {
    (void)param;
    TickType_t last_wake = xTaskGetTickCount();

    for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
        sample_t s;
        fake_sensor_read(i, &s);
        if (xQueueSend(g_sample_queue, &s, portMAX_DELAY) == pdPASS) {
            console_printf("[sampler]   sent #%u (az=%d mg)\n",
                           (unsigned)(i + 1u), (int)s.az);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }

    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void processor_task(void *param) {
    (void)param;
    int32_t window[4] = { 0, 0, 0, 0 };
    size_t count = 0;

    for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
        sample_t s;
        if (xQueueReceive(g_sample_queue, &s, portMAX_DELAY) == pdPASS) {
            window[count % 4u] = s.az;
            count++;
            size_t valid = count < 4u ? count : 4u;
            int32_t sum = 0;
            for (size_t k = 0; k < valid; ++k) {
                sum += window[k];
            }
            console_printf("[processor] recv #%u: ax=%d ay=%d az=%d -> avg_az=%d mg\n",
                           (unsigned)count, (int)s.ax, (int)s.ay, (int)s.az,
                           (int)(sum / (int32_t)valid));
        }
    }

    sim_finish();
}

int main(void) {
    g_sample_queue = xQueueCreate(4, sizeof(sample_t));
    if (g_sample_queue == NULL) {
        return 1;
    }

    if (xTaskCreate(sampler_task, "sampler", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(processor_task, "proc", 256, NULL, 1, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}
