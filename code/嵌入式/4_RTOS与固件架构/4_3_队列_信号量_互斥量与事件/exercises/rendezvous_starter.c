/*
 * exercises/rendezvous_starter.c：习题 60434 初始代码（用 xEventGroupSync 实现多任务会合）。
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "../sim_support.h"

#define BIT_TASK_0 ( 1UL << 0 )
#define BIT_TASK_1 ( 1UL << 1 )
#define BIT_TASK_2 ( 1UL << 2 )
#define ALL_SYNC_BITS ( BIT_TASK_0 | BIT_TASK_1 | BIT_TASK_2 )

static EventGroupHandle_t g_sync_group;

typedef struct {
    const char *name;
    TickType_t prep_ms;
    EventBits_t my_bit;
} worker_spec_t;

static const worker_spec_t g_specs[3] = {
    { "worker0", 10, BIT_TASK_0 },
    { "worker1", 25, BIT_TASK_1 },
    { "worker2", 40, BIT_TASK_2 },
};

static void worker_task(void *param) {
    const worker_spec_t *spec = (const worker_spec_t *)param;

    vTaskDelay(pdMS_TO_TICKS(spec->prep_ms));
    console_printf("[%s] preparation done at %u ms, waiting at barrier\n",
                   spec->name, (unsigned)xTaskGetTickCount());

    /* TODO: 调用 xEventGroupSync 设置 spec->my_bit 并等待 ALL_SYNC_BITS */

    console_printf("[%s] passed barrier at %u ms\n",
                   spec->name, (unsigned)xTaskGetTickCount());

    if (spec->my_bit == BIT_TASK_2) {
        sim_finish();
    }
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void) {
    g_sync_group = xEventGroupCreate();
    if (g_sync_group == NULL) {
        return 1;
    }

    for (size_t i = 0; i < 3; ++i) {
        xTaskCreate(worker_task, g_specs[i].name, 256, (void *)&g_specs[i], 2, NULL);
    }

    vTaskStartScheduler();
    return 0;
}
