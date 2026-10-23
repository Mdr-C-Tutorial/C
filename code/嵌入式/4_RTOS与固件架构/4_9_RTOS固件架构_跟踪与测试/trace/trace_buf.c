/*
 * trace_buf.c：用 traceTASK_SWITCHED_IN 把“时间戳 + 任务名”写进 RAM 环形缓冲区，
 * 运行结束后打印成时间线。时间戳用高精度计数器，目标版换成 DWT->CYCCNT。
 *
 * 本文件自带跟踪钩子的实现，构建时定义 TRACE_OWN_HOOK，
 * 不让 common/sim_hooks.c 再给一份空实现。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

#define TRACE_DEPTH 48

struct trace_entry {
    uint64_t time;
    const char *name;
};

static struct trace_entry g_trace[TRACE_DEPTH];
static volatile uint32_t g_trace_count;

void sim_trace_switch_in(void *task) {
    uint32_t index = g_trace_count;

    if (index < TRACE_DEPTH) {
        g_trace[index].time = sim_run_time_counter();
        g_trace[index].name = pcTaskGetName((TaskHandle_t)task);
    }
    g_trace_count = index + 1;
}

static void worker_a(void *param) {
    (void)param;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void worker_b(void *param) {
    (void)param;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void report_task(void *param) {
    (void)param;
    vTaskDelay(pdMS_TO_TICKS(300));

    /* 关中断抓一份快照，避免打印过程中又被写 */
    taskENTER_CRITICAL();
    uint32_t n = g_trace_count < TRACE_DEPTH ? g_trace_count : TRACE_DEPTH;
    uint64_t t0 = n > 0 ? g_trace[0].time : 0;
    taskEXIT_CRITICAL();

    console_printf("entries=%lu (buffer %d)\n", (unsigned long)g_trace_count, TRACE_DEPTH);
    console_printf("   t/us  task\n");
    console_printf("--------  ------\n");
    for (uint32_t i = 0; i < n; ++i) {
        console_printf("%8llu  %s\n",
                       (unsigned long long)((g_trace[i].time - t0) / 10u),
                       g_trace[i].name);
    }

    sim_finish();
}

int main(void) {
    console_init();
    xTaskCreate(worker_a, "worker_a", 256, NULL, 2, NULL);
    xTaskCreate(worker_b, "worker_b", 256, NULL, 2, NULL);
    xTaskCreate(report_task, "report", 1024, NULL, 3, NULL);
    vTaskStartScheduler();
    return 0;
}
