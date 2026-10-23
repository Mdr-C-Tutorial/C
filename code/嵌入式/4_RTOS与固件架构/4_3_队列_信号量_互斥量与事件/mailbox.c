/*
 * mailbox.c：用长度为 1 的队列配合 xQueueOverwrite / xQueuePeek 保存最新状态。
 *
 * 讲解见教程 4.3 节「队列、信号量、互斥量与事件」。
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "sim_support.h"

typedef struct {
    uint32_t seq;
    uint16_t sample_rate_hz;
    int16_t alarm_threshold_mg;
} sensor_cfg_t;

static QueueHandle_t g_cfg_mailbox;

static void cfg_writer_task(void *param) {
    (void)param;
    static const sensor_cfg_t updates[] = {
        { 1u, 100u, 1200 },
        { 2u, 200u, 1500 },
        { 3u, 500u, 1800 },
    };

    for (size_t i = 0; i < sizeof(updates) / sizeof(updates[0]); ++i) {
        xQueueOverwrite(g_cfg_mailbox, &updates[i]);
        console_printf("[writer] overwrite cfg seq=%u rate=%u Hz th=%d mg\n",
                       (unsigned)updates[i].seq,
                       (unsigned)updates[i].sample_rate_hz,
                       (int)updates[i].alarm_threshold_mg);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void cfg_reader_task(void *param) {
    const char *name = (const char *)param;

    for (int i = 0; i < 3; ++i) {
        vTaskDelay(pdMS_TO_TICKS(15));
        sensor_cfg_t cfg;
        if (xQueuePeek(g_cfg_mailbox, &cfg, portMAX_DELAY) == pdPASS) {
            console_printf("[%s] peek cfg seq=%u rate=%u Hz th=%d mg\n",
                           name,
                           (unsigned)cfg.seq,
                           (unsigned)cfg.sample_rate_hz,
                           (int)cfg.alarm_threshold_mg);
        }
    }

    if (name[0] == 'u') {
        sim_finish();
    }
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void) {
    g_cfg_mailbox = xQueueCreate(1, sizeof(sensor_cfg_t));
    if (g_cfg_mailbox == NULL) {
        return 1;
    }

    if (xTaskCreate(cfg_writer_task, "writer", 256, NULL, 3, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(cfg_reader_task, "ctrl", 256, (void *)"ctrl", 2, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(cfg_reader_task, "ui", 256, (void *)"ui  ", 1, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}
