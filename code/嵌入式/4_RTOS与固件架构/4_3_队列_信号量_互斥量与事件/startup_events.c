/*
 * startup_events.c：用事件组等待传感器、存储与网络三项子系统全部就绪。
 *
 * 讲解见教程 4.3 节「队列、信号量、互斥量与事件」。
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "sim_support.h"

#define BIT_SENSOR_READY  ( 1UL << 0 )
#define BIT_STORAGE_READY ( 1UL << 1 )
#define BIT_NET_READY     ( 1UL << 2 )
#define ALL_READY_BITS    ( BIT_SENSOR_READY | BIT_STORAGE_READY | BIT_NET_READY )

static EventGroupHandle_t g_startup_events;

typedef struct {
    const char *name;
    TickType_t init_ms;
    EventBits_t bit;
} subsys_info_t;

static const subsys_info_t g_subsys[] = {
    { "sensor ", 10, BIT_SENSOR_READY  },
    { "storage", 25, BIT_STORAGE_READY },
    { "net    ", 40, BIT_NET_READY     },
};

static void init_task(void *param) {
    const subsys_info_t *info = (const subsys_info_t *)param;
    vTaskDelay(pdMS_TO_TICKS(info->init_ms));
    console_printf("[init:%s] ready, setting bit 0x%02x\n",
                   info->name, (unsigned)info->bit);
    xEventGroupSetBits(g_startup_events, info->bit);

    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void app_worker_task(void *param) {
    const char *role = (const char *)param;
    EventBits_t bits = xEventGroupWaitBits(
        g_startup_events,
        ALL_READY_BITS,
        pdFALSE,        /* 退出时不清除就绪位，其他等待任务也能看到 */
        pdTRUE,         /* 等待全部 3 个位同时置位（逻辑与） */
        portMAX_DELAY);

    if ((bits & ALL_READY_BITS) == ALL_READY_BITS) {
        console_printf("[app:%s] all subsystems ready (bits=0x%02x), entering RUN state\n",
                       role, (unsigned)(bits & ALL_READY_BITS));
    }

    if (role[0] == 't') {
        sim_finish();
    }
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void) {
    g_startup_events = xEventGroupCreate();
    if (g_startup_events == NULL) {
        return 1;
    }

    for (size_t i = 0; i < sizeof(g_subsys) / sizeof(g_subsys[0]); ++i) {
        if (xTaskCreate(init_task, "init", 256, (void *)&g_subsys[i], 2, NULL) != pdPASS) {
            return 1;
        }
    }
    if (xTaskCreate(app_worker_task, "ctrl", 256, (void *)"control  ", 3, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(app_worker_task, "tel", 256, (void *)"telemetry", 1, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}
