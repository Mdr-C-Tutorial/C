/*
 * fake_tick.c：测试构建里替代真实 tick 实现。
 *
 * 时间由测试代码直接设置，不依赖任何定时器。
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#include <stdint.h>

#include "tick.h"

static uint32_t g_now;

uint32_t tick_now(void) {
    return g_now;
}

void fake_tick_set(uint32_t ms) {
    g_now = ms;
}
