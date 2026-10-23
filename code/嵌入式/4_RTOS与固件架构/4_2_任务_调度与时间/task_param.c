/*
 * task_param.c：同一个任务函数用不同参数创建两个实例。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

typedef struct {
    const char *name;
    uint32_t period_ms;
    uint32_t rounds;
} worker_cfg_t;

static void worker_task(void *param) {
    const worker_cfg_t *cfg = (const worker_cfg_t *)param;
    TickType_t last_wake = xTaskGetTickCount();

    for (uint32_t i = 1; i <= cfg->rounds; ++i) {
        console_printf("[%s] round %u at tick %u\n",
                       cfg->name, (unsigned)i, (unsigned)xTaskGetTickCount());
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(cfg->period_ms));
    }

    if (cfg->name[0] == 's') {
        sim_finish();
    }
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void) {
    static const worker_cfg_t fast = { "fast", 100, 5 };
    static const worker_cfg_t slow = { "slow", 250, 2 };

    console_init();

    xTaskCreate(worker_task, "fast", 256, (void *)&fast, 2, NULL);
    xTaskCreate(worker_task, "slow", 256, (void *)&slow, 1, NULL);

    vTaskStartScheduler();
    return 0;
}
