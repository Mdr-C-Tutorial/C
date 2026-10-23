/*
 * rtos_console_sim.c：模拟器上的控制台实现。
 *
 * 整个 vprintf + fflush 放在临界区里，避免两个任务各打印半行、输出交错。
 * Windows 移植层下这也是保护 C 运行库的必要做法，FreeRTOS 自带的 Windows
 * 演示程序同样如此。
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

void console_init(void) {
}

void console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    taskENTER_CRITICAL();
    vprintf(fmt, args);
    fflush(stdout);
    taskEXIT_CRITICAL();
    va_end(args);
}

void sim_finish(void) {
    taskENTER_CRITICAL();
    fflush(stdout);
    exit(0);
}
