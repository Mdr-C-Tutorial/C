#include "tick.h"
#include "stm32f1xx.h"

static volatile uint32_t g_tick_ms;

void SysTick_Handler(void) {
    ++g_tick_ms;
}

int tick_init(uint32_t sysclk_hz) {
    g_tick_ms = 0u;
    if (SysTick_Config(sysclk_hz / 1000u) != 0u) {
        return -1;
    }
    return 0;
}

uint32_t tick_now(void) {
    return g_tick_ms;
}

bool tick_elapsed(uint32_t start, uint32_t interval_ms) {
    return (uint32_t)(tick_now() - start) >= interval_ms;
}
