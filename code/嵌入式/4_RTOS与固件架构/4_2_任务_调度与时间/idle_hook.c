/*
 * idle_hook.c：空闲任务与空闲钩子。
 *
 * 调度器启动时自动创建优先级 0 的空闲任务。把 configUSE_IDLE_HOOK 打开后，
 * 空闲任务每循环一次调用一次 vApplicationIdleHook()，这里在里面累加计数，
 * 过一段时间打印，用来说明“CPU 什么时候是闲的”。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

static volatile uint32_t g_idle_loops;

/* 由内核在空闲任务里调用；不能阻塞，也不能调用会阻塞的 API */
void vApplicationIdleHook(void) {
    g_idle_loops++;
}

static void reporter_task(void *param) {
    (void)param;

    for (unsigned round = 1; round <= 3; ++round) {
        vTaskDelay(pdMS_TO_TICKS(200));
        console_printf("[report] idle loops so far: %u\n",
                       (unsigned)g_idle_loops);
    }

    sim_finish();
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void worker_task(void *param) {
    (void)param;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

int main(void) {
    console_init();

    xTaskCreate(reporter_task, "report", 256, NULL, 2, NULL);
    xTaskCreate(worker_task, "work", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    return 0;
}
