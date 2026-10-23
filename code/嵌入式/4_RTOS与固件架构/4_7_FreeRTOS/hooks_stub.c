/*
 * hooks_stub.c：钩子的空实现。
 *
 * 本节打开的开关要求工程提供这几个函数。只演示定时器或任务表的示例
 * 链接这一份空实现，hooks_sim.c 则自己定义全部钩子。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

void vApplicationIdleHook(void) {
}

void vApplicationTickHook(void) {
}

void vApplicationMallocFailedHook(void) {
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    (void)name;
    for (;;) {
    }
}
