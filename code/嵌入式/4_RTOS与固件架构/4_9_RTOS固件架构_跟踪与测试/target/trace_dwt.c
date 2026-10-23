/*
 * trace_dwt.c：目标上用 DWT 周期计数器给跟踪记录打时间戳。
 *
 * DWT 的 CYCCNT 在 Cortex-M3 上随内核时钟递增，72 MHz 下 32 位计数值
 * 约 59.6 秒回绕一次。演示用 DWT 寄存器的地址来自 PM0056（0xE0001000）。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#define DEMCR      (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)

#define DEMCR_TRCENA       (1u << 24)
#define DWT_CTRL_CYCCNTENA (1u << 0)

void dwt_timestamp_init(void) {
    DEMCR |= DEMCR_TRCENA;
    DWT_CYCCNT = 0;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;
}

static inline uint32_t dwt_cycles(void) {
    return DWT_CYCCNT;
}

/* 与模拟器版相同的数据结构，时间戳换成 32 位周期数 */
#define TRACE_DEPTH 64

struct trace_entry {
    uint32_t cycles;
    const char *name;
};

static struct trace_entry g_trace[TRACE_DEPTH];
static volatile uint32_t g_trace_count;

void target_trace_switch_in(void *task) {
    uint32_t index = g_trace_count;

    if (index < TRACE_DEPTH) {
        g_trace[index].cycles = dwt_cycles();
        g_trace[index].name = pcTaskGetName((TaskHandle_t)task);
    }
    g_trace_count = index + 1;
}

/* 72 MHz 下一个周期约 13.9 ns；这里只做整微秒换算 */
uint32_t trace_cycles_to_us(uint32_t cycles) {
    return cycles / 72u;
}
