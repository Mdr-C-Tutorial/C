/*
 * sim_hooks.c：模拟器上的钩子、运行时统计计数器与跟踪钩子。
 *
 * 运行时统计的计数器用 Windows 的高精度性能计数器（QPC），频率约 10 MHz，
 * 是 1000 Hz 节拍的 10000 倍。QPC 在系统启动时就开始计数，所以调度器启动前
 * 先记下当时的读数，之后一律返回相对它的差值，让统计从 0 开始。
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "FreeRTOS.h"
#include "task.h"

static uint64_t g_counter_base;

/* 配置没有打开空闲钩子，但 4.2 的公共模板打开了；两边都编时这一份兜底 */
void vApplicationIdleHook(void) {
}

void vApplicationMallocFailedHook(void) {
    taskENTER_CRITICAL();
    printf("[hook] malloc failed\n");
    fflush(stdout);
    taskEXIT_CRITICAL();
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    taskENTER_CRITICAL();
    printf("[hook] stack overflow in %s\n", name);
    fflush(stdout);
    taskEXIT_CRITICAL();
    for (;;) {
    }
}

void run_time_stats_init(void) {
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    g_counter_base = (uint64_t)now.QuadPart;
}

uint64_t sim_run_time_counter(void) {
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (uint64_t)now.QuadPart - g_counter_base;
}

/* 跟踪钩子只在 trace_buf 里用到；其他示例把它编成空函数 */
#if !defined(TRACE_OWN_HOOK)
void sim_trace_switch_in(void *task) {
    (void)task;
}
#endif
