/*
 * notify_counter.c：用任务通知（xTaskNotifyGive / ulTaskNotifyTake）替代轻量计数信号量。
 *
 * 讲解见教程 4.3 节「队列、信号量、互斥量与事件」。
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "sim_support.h"

static TaskHandle_t g_consumer_handle;

static void producer_task(void *param) {
    (void)param;

    console_printf("[producer] burst of 3 events -> xTaskNotifyGive x3\n");
    xTaskNotifyGive(g_consumer_handle);
    xTaskNotifyGive(g_consumer_handle);
    xTaskNotifyGive(g_consumer_handle);

    vTaskDelay(pdMS_TO_TICKS(30));

    console_printf("[producer] single event -> xTaskNotifyGive x1\n");
    xTaskNotifyGive(g_consumer_handle);

    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void consumer_task(void *param) {
    (void)param;
    unsigned processed = 0;

    while (processed < 4u) {
        /* 第一个参数传 pdFALSE：每次取走一个计数（通知值减 1），与计数信号量行为一致 */
        uint32_t prev_count = ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        if (prev_count > 0u) {
            processed++;
            console_printf("[consumer] took 1 event (count before take=%u, total=%u)\n",
                           (unsigned)prev_count, processed);
        }
    }

    sim_finish();
}

int main(void) {
    if (xTaskCreate(consumer_task, "consumer", 256, NULL, 1, &g_consumer_handle) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(producer_task, "producer", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}
