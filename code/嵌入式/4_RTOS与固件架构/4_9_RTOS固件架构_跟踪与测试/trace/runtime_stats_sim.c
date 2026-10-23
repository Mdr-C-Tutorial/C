/*
 * runtime_stats_sim.c：configGENERATE_RUN_TIME_STATS 下打印各任务的 CPU 占比。
 *
 * 空闲任务的占比就是空闲 CPU 的比例。模拟器不是实时系统，数字只作演示。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

static void sampler_task(void *param) {
    (void)param;
    for (;;) {
        /* 模拟一次采样与滤波：可观的 CPU 时间 */
        volatile uint32_t acc = 0;
        for (uint32_t i = 0; i < 400000u; ++i) {
            acc += i;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void logger_task(void *param) {
    (void)param;
    for (;;) {
        volatile uint32_t acc = 0;
        for (uint32_t i = 0; i < 50000u; ++i) {
            acc += i;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void report_task(void *param) {
    char buf[1024];

    (void)param;
    vTaskDelay(pdMS_TO_TICKS(3000));

    vTaskGetRunTimeStats(buf);
    console_printf("Task            Abs Time        %% Time\n");
    console_printf("----------------------------------------\n");
    console_printf("%s", buf);
    sim_finish();
}

int main(void) {
    console_init();
    xTaskCreate(sampler_task, "sampler", 512, NULL, 3, NULL);
    xTaskCreate(logger_task, "logger", 256, NULL, 2, NULL);
    xTaskCreate(report_task, "report", 1024, NULL, 4, NULL);
    vTaskStartScheduler();
    return 0;
}