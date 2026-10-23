/*
 * FreeRTOSConfig_stm32.h：STM32F103C8（Cortex-M3，ARM_CM3 移植层）使用的 FreeRTOS 配置。
 * 构建时由 freertos.cmake 复制为构建目录里的 FreeRTOSConfig.h。
 *
 * 讲解见教程 4.2 节「任务、调度与时间」，逐项解释在 4.7 节。
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
/* 4.7 的 tickless 示例用 -DconfigUSE_TICKLESS_IDLE=2 覆盖这一项 */
#ifndef configUSE_TICKLESS_IDLE
#define configUSE_TICKLESS_IDLE                 0
#endif
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 72000000 )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 10 * 1024 ) )
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                8
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/* 断言：失败时关中断并停住，接调试器就能看到调用栈（4.7 节展开） */
#define configASSERT( x )     do { if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ) {} } } while( 0 )

#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_MALLOC_FAILED_HOOK            0

#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1

/* 中断优先级：Cortex-M3 数值越小越高。4 位有效位左移 4 位存放。
 * 优先级数值 >= 5 的中断可以被内核屏蔽，也只有它们能调用 FromISR API（4.4）。 */
#define configKERNEL_INTERRUPT_PRIORITY         ( 15u << 4 )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     ( 5u << 4 )

/* 让 ST 启动文件里的三个处理函数指向内核实现（V11 默认检查是否装对） */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
