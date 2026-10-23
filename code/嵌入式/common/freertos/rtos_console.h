/*
 * rtos_console.h：第 4 章示例共用的控制台输出接口。
 *
 * 模拟器上由 rtos_console_sim.c 实现（临界区保护下的 vprintf），
 * STM32F103 上由 rtos_console_uart.c 实现（USART1，115200 8N1）。
 * 应用代码只调用 console_init() 与 console_printf()，两种构建源码相同。
 *
 * 讲解见教程 4.2 节「任务、调度与时间」。
 */
#ifndef RTOS_CONSOLE_H
#define RTOS_CONSOLE_H

#include "FreeRTOS.h"
#include "task.h"

/* 目标构建里初始化 USART1；模拟器里不做任何事。 */
void console_init(void);

/* 线程安全地格式化输出；参数与 printf 相同。 */
void console_printf(const char *fmt, ...);

#if defined(__ARM_ARCH)
/* 目标上没有进程可退出：示例收尾时让任务永久阻塞，调度器继续运行 */
static inline void sim_finish(void) {
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}
#else
/* 模拟器示例收尾：把标准输出刷干净后结束整个进程 */
void sim_finish(void);
#endif

#endif /* RTOS_CONSOLE_H */
