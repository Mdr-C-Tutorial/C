/*
 * two_tasks.c：最小的双任务程序，两个任务交替打印。
 *
 * 模拟器上运行：
 *   cmake -B build -G Ninja -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录>
 *   cmake --build build
 *   ./build/two_tasks
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

static void blink_task(void *param) {
    (void)param;

    for (unsigned round = 1; round <= 3; ++round) {
        console_printf("[blink ] round %u, LED toggled\n", round);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void report_task(void *param) {
    (void)param;

    for (unsigned round = 1; round <= 3; ++round) {
        console_printf("[report] round %u at tick %u\n",
                       round, (unsigned)xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    sim_finish();
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void) {
    console_init();

    xTaskCreate(blink_task, "blink", 256, NULL, 1, NULL);
    xTaskCreate(report_task, "report", 256, NULL, 2, NULL);

    vTaskStartScheduler();

    /* 调度器启动后不会再回到这里 */
    return 0;
}
