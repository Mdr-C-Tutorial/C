/*
 * delay_drift.c：vTaskDelay 的相对延时与 xTaskDelayUntil 的绝对延时对比。
 *
 * 同一个任务先做 5 轮“工作 10 ms + 自称 100 ms 周期”，用 vTaskDelay；
 * 再做 5 轮同样的工作，用 xTaskDelayUntil。打印每次醒来时的节拍值。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

/* 模拟每轮 10 ms 的工作耗时 */
static void do_work(void) {
    vTaskDelay(pdMS_TO_TICKS(10));
}

static void periodic_task(void *param) {
    (void)param;

    console_printf("--- vTaskDelay(100) ---\n");
    for (unsigned round = 1; round <= 5; ++round) {
        do_work();
        console_printf("[delay] round %u wakes at tick %u\n",
                       round, (unsigned)xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    console_printf("--- xTaskDelayUntil(last_wake, 100) ---\n");
    TickType_t last_wake = xTaskGetTickCount();
    for (unsigned round = 1; round <= 5; ++round) {
        do_work();
        console_printf("[until] round %u wakes at tick %u\n",
                       round, (unsigned)xTaskGetTickCount());
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }

    sim_finish();
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void) {
    console_init();

    xTaskCreate(periodic_task, "periodic", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    return 0;
}
