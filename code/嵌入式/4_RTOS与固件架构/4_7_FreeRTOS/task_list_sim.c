/*
 * task_list_sim.c：用 vTaskList 打印任务表。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

static void worker_task(void *param) {
    const char *name = (const char *)param;

    for (unsigned i = 0; i < 3; ++i) {
        console_printf("[%s] step %u\n", name, i + 1);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    vTaskDelay(portMAX_DELAY);
}

static void report_task(void *param) {
    char table[512];

    (void)param;
    vTaskDelay(pdMS_TO_TICKS(100));

    vTaskList(table);
    console_printf("Name           State  Prio  Stack  Num\n");
    console_printf("--------------------------------------\n");
    console_printf("%s", table);
    console_printf("\nuxTaskGetNumberOfTasks() = %u\n",
                   (unsigned)uxTaskGetNumberOfTasks());
    sim_finish();
}

int main(void) {
    console_init();
    xTaskCreate(worker_task, "sampler", 256, "sampler", 2, NULL);
    xTaskCreate(worker_task, "logger", 256, "logger", 1, NULL);
    xTaskCreate(report_task, "report", 512, NULL, 3, NULL);
    vTaskStartScheduler();
    return 0;
}
