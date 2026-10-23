/*
 * target_hooks.c：STM32F103 目标构建里的钩子。
 *
 * 目标上打开的是 configCHECK_FOR_STACK_OVERFLOW 与 malloc 失败钩子，
 * 调试构建让它们停在断点上；正式固件换成 6.2 的故障记录。
 * 运行时统计与跟踪在目标上另行接入（6.5 讲 DWT 的细节）。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

void vApplicationMallocFailedHook(void) {
    __asm volatile("bkpt 0");
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    (void)name;
    __asm volatile("bkpt 0");
    for (;;) {
    }
}
