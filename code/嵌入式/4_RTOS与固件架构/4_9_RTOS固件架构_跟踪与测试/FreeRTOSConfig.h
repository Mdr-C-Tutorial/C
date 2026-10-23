/*
 * FreeRTOSConfig.h：4.9 节示例的配置。
 *
 * 相比前几页，多打开三项：configUSE_TRACE_FACILITY（vTaskList 与
 * uxTaskGetSystemState）、configUSE_STATS_FORMATTING_FUNCTIONS（vTaskGetRunTimeStats
 * 的格式化）、configGENERATE_RUN_TIME_STATS（运行时统计）。
 *
 * 讲解见教程 4.9 节「RTOS 固件架构、跟踪与测试」。
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      ((unsigned long)72000000)
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    7
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

#if defined(__ARM_ARCH)
#define configMINIMAL_STACK_SIZE                ((unsigned short)128)
#define configTOTAL_HEAP_SIZE                   ((size_t)(10 * 1024))
#else
#define configMINIMAL_STACK_SIZE                ((unsigned short)256)
#define configTOTAL_HEAP_SIZE                   ((size_t)(128 * 1024))
#endif

#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_TIME_SLICING                  1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                8
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1

/* 本页的主角 */
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1
/* vTaskGetRunTimeStats 是宏，缓冲区长度由这一项给（V11 起） */
#define configSTATS_BUFFER_MAX_LENGTH           1024

/* 运行时统计：counter 由工程提供，模拟器用高精度计数器，目标用 DWT->CYCCNT */
#if defined(__ARM_ARCH)
#define configGENERATE_RUN_TIME_STATS           0
#else
#define configGENERATE_RUN_TIME_STATS           1
#define configRUN_TIME_COUNTER_TYPE             uint64_t
extern uint64_t sim_run_time_counter(void);
#define portGET_RUN_TIME_COUNTER_VALUE()        sim_run_time_counter()
extern void run_time_stats_init(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() run_time_stats_init()
#endif

/* 跟踪钩子：sim_trace.c 里记时间戳与任务名 */
#if defined(__ARM_ARCH)
extern void target_trace_switch_in(void *task);
#define traceTASK_SWITCHED_IN()                 target_trace_switch_in((void *)pxCurrentTCB)
#else
extern void sim_trace_switch_in(void *task);
#define traceTASK_SWITCHED_IN()                 sim_trace_switch_in((void *)pxCurrentTCB)
#endif

#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1

#if defined(__ARM_ARCH)
#define configKERNEL_INTERRUPT_PRIORITY         (15u << 4)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5u << 4)

#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
#endif

#endif /* FREERTOS_CONFIG_H */
