#include "tick.h"
#include "stm32f1xx.h"

static volatile uint32_t g_ms = 0;

void SysTick_Handler(void) {
    ++g_ms;
}

void tick_init(void) {
    /* SystemCoreClock = 72000000，每 72000 个周期（1 ms）触发一次中断 */
    SysTick_Config(SystemCoreClock / 1000u);
}

uint32_t tick_now(void) {
    return g_ms;
}

int tick_elapsed(uint32_t start, uint32_t interval_ms) {
    return (uint32_t)(tick_now() - start) >= interval_ms;
}

void delay_ms(uint32_t ms) {
    uint32_t start = tick_now();
    while ((uint32_t)(tick_now() - start) < ms) {
    }
}
