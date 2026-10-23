/*
 * rtconfig.h：本节示例的最小内核配置。
 *
 * 真实工程由 Env 的 menuconfig 根据 Kconfig 生成，几百个选项；
 * 这里只保留编译内核与示例必需的部分。
 */
#ifndef RT_CONFIG_H__
#define RT_CONFIG_H__

#define RT_NAME_MAX                 8
#define RT_ALIGN_SIZE               4
#define RT_THREAD_PRIORITY_MAX      8
#define RT_TICK_PER_SECOND          1000
#define RT_CPUS_NR                  1
#define RT_BACKTRACE_LEVEL_MAX_NR   32

#define RT_USING_OVERFLOW_CHECK
#define RT_DEBUG
#define RT_USING_HOOK
#define RT_USING_IDLE_HOOK
#define RT_IDLE_HOOK_LIST_SIZE      4
#define IDLE_THREAD_STACK_SIZE      256

#define RT_USING_TIMER_SOFT
#define RT_TIMER_THREAD_PRIO        4
#define RT_TIMER_THREAD_STACK_SIZE  512
#define RT_TIMER_TICK_PER_SECOND    1000

#define RT_USING_SEMAPHORE
#define RT_USING_MUTEX
#define RT_USING_EVENT
#define RT_USING_MAILBOX
#define RT_USING_MESSAGEQUEUE

#define RT_USING_MEMPOOL
#define RT_USING_SMALL_MEM
#define RT_USING_HEAP

#define RT_USING_COMPONENTS_INIT
#define RT_USING_USER_MAIN
#define RT_MAIN_THREAD_STACK_SIZE   1024
#define RT_MAIN_THREAD_PRIORITY     3

#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE          128

#endif /* RT_CONFIG_H__ */