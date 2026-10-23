/*
 * exercises/multi_producer_starter.c：习题 60432 初始代码（两生产者一消费者队列）。
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "../sim_support.h"

typedef struct {
    uint8_t source_id;
    TickType_t tick;
    int16_t value;
} tagged_sample_t;

static QueueHandle_t g_queue;

typedef struct {
    uint8_t source_id;
    TickType_t period_ms;
    int16_t base_value;
} producer_cfg_t;

static const producer_cfg_t g_cfg_a = { 1u, 10u, 100 };
static const producer_cfg_t g_cfg_b = { 2u, 15u, 200 };

static void producer_task(void *param) {
    const producer_cfg_t *cfg = (const producer_cfg_t *)param;
    TickType_t last_wake = xTaskGetTickCount();
    int16_t seq = 0;

    for (;;) {
        tagged_sample_t item = {
            .source_id = cfg->source_id,
            .tick = xTaskGetTickCount(),
            .value = (int16_t)(cfg->base_value + seq),
        };
        seq++;
        /* TODO: 把 item 发送到 g_queue，再按 cfg->period_ms 周期延时 */
        (void)item;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(cfg->period_ms));
    }
}

static void consumer_task(void *param) {
    (void)param;
    unsigned count_src1 = 0;
    unsigned count_src2 = 0;

    for (unsigned total = 0; total < 10u; ++total) {
        tagged_sample_t item = { 0 };
        /* TODO: 从 g_queue 接收样本，按 source_id 统计并打印 */
        (void)item;
    }

    console_printf("summary: src1=%u, src2=%u\n", count_src1, count_src2);
    sim_finish();
}

int main(void) {
    g_queue = xQueueCreate(8, sizeof(tagged_sample_t));
    if (g_queue == NULL) {
        return 1;
    }

    xTaskCreate(producer_task, "prodA", 256, (void *)&g_cfg_a, 2, NULL);
    xTaskCreate(producer_task, "prodB", 256, (void *)&g_cfg_b, 2, NULL);
    xTaskCreate(consumer_task, "cons",  256, NULL,             1, NULL);

    vTaskStartScheduler();
    return 0;
}
