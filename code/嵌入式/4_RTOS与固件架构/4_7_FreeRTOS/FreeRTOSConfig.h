/*
 * FreeRTOSConfig.h：4.7 节示例共用的配置。
 *
 * 模拟器与 STM32F103 目标都编这一份：与芯片相关的几项按 __ARM_ARCH 区分，
 * 其余是本节要演示的开关（软件定时器、四个钩子、任务表）。
 *
 * 讲解见教程 4.7 节「FreeRTOS」。
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

/* tickless_stop 的构建用 -DconfigUSE_TICKLESS_IDLE=2 覆盖 */
#ifndef configUSE_TICKLESS_IDLE
#define configUSE_TICKLESS_IDLE                 0
#endif

#define configCPU_CLOCK_HZ                      ((unsigned long)72000000)
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    7
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

/* 目标上只有 20 KiB SRAM；模拟器上用大堆，队列与任务都够用 */
#if defined(__ARM_ARCH)
#define configMINIMAL_STACK_SIZE                ((unsigned short)128)
#define configTOTAL_HEAP_SIZE                   ((size_t)(10 * 1024))
#else
#define configMINIMAL_STACK_SIZE                ((unsigned short)256)
#define configTOTAL_HEAP_SIZE                   ((size_t)(128 * 1024))
#endif

#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                8
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* 参数检查：断言失败时关中断停住，接调试器时换成 __asm volatile("bkpt 0") */
#define configASSERT(x)                                                      \
    do {                                                                     \
        if ((x) == 0) {                                                      \
            taskDISABLE_INTERRUPTS();                                        \
            for (;;) {                                                       \
            }                                                                \
        }                                                                    \
    } while (0)

#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1

/* vTaskList 需要的两个开关 */
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

#if defined(__ARM_ARCH)
/* Cortex-M3：4 位有效优先级左移 4 位存放（4.7 第 3 节） */
#define configKERNEL_INTERRUPT_PRIORITY         (15u << 4)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5u << 4)

/* 让 ST 启动文件里的三个处理函数直接指向内核实现 */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
#endif

#endif /* FREERTOS_CONFIG_H */
