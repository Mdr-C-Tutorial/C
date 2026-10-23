/*
 * shared_counter_starter.c：习题 62242 起始代码。
 *
 * 任务：补全 pulse_capture_on_isr() 与 pulse_take_and_clear()，消除主循环读出并清零
 * 计数器时与中断服务程序之间的竞态条件，同时保持最小关中断窗口。
 */
#include <stdint.h>
#include "stm32f1xx.h"

static volatile uint32_t g_pulse_count;
static volatile uint32_t g_pulse_max_interval;

void pulse_capture_on_isr(uint32_t interval_ticks) {
    /* TODO: 累加脉冲计数，并在 interval_ticks 更大时更新最大间隔 */
    (void)interval_ticks;
}

uint32_t pulse_take_and_clear(uint32_t *out_max_interval) {
    /* TODO: 使用 PRIMASK 临界区原子地取出 g_pulse_count 和 g_pulse_max_interval 并清零 */
    if (out_max_interval != 0) {
        *out_max_interval = g_pulse_max_interval;
    }
    return g_pulse_count;
}
