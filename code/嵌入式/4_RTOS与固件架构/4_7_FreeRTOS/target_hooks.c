/*
 * target_hooks.c：STM32F103 目标构建里的钩子与 configASSERT 的落点。
 *
 * 本节配置打开了空闲钩子、节拍钩子、栈溢出检查与 malloc 失败钩子，
 * 内核要求工程提供这四个函数。目标上不打印，调试构建直接停在断点上；
 * 正式固件里换成 6.2 的故障记录，把现场写进 .noinit 再复位。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

void vApplicationIdleHook(void) {
}

void vApplicationTickHook(void) {
}

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
