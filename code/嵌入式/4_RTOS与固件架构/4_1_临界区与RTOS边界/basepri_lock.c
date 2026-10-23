/*
 * basepri_lock.c：对比 BASEPRI 临界区与 PRIMASK 临界区生成的机器指令。
 */
#include "basepri_lock.h"

/* 屏蔽优先级数值 >= 5 的中断（STM32F1 实现高 4 位，左移 4 位为 0x50） */
#define LOG_MASK_PRIO (5u << (8u - __NVIC_PRIO_BITS))

volatile uint32_t g_log_dropped;
volatile uint32_t g_log_written;

void log_counter_add_basepri(uint32_t delta) {
    uint32_t saved = basepri_lock(LOG_MASK_PRIO);
    g_log_written += delta;
    basepri_unlock(saved);
}

void log_counter_add_primask(uint32_t delta) {
    uint32_t saved = primask_lock();
    g_log_written += delta;
    primask_unlock(saved);
}
