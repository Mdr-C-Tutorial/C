/*
 * basepri_lock.h：基于 Cortex-M3 BASEPRI 寄存器的可嵌套按优先级屏蔽临界区。
 *
 * 注意：
 *   1. priority_threshold 为左移对齐后的 8 位硬件优先级值，
 *      在 STM32F1（高 4 位有效）上可用 (prio << (8u - __NVIC_PRIO_BITS)) 构造。
 *   2. 进入临界区时使用 MSR basepri_max，仅当新阈值优先级高于当前屏蔽阈值
 *      （即非零且数值更小，或当前 BASEPRI 为 0）时才更新硬件寄存器，
 *      防止内层嵌套误把外层更高的屏蔽级别放宽。
 */
#ifndef BASEPRI_LOCK_H
#define BASEPRI_LOCK_H

#include <stdint.h>
#include "stm32f1xx.h"

static inline uint32_t basepri_lock(uint32_t priority_threshold) {
    uint32_t saved = __get_BASEPRI();
    __set_BASEPRI_MAX(priority_threshold);
    __DSB();
    __ISB();
    return saved;
}

static inline void basepri_unlock(uint32_t saved) {
    __set_BASEPRI(saved);
}

static inline uint32_t primask_lock(void) {
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    return saved;
}

static inline void primask_unlock(uint32_t saved) {
    __set_PRIMASK(saved);
}

void log_counter_add_basepri(uint32_t delta);
void log_counter_add_primask(uint32_t delta);

#endif
