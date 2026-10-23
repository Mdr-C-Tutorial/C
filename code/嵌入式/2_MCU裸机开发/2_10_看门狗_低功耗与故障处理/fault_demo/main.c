#include "clock.h"
#include "console.h"
#include "fault.h"
#include "stm32f1xx.h"
#include <stdint.h>

static volatile uint32_t g_sink;

static void dummy_target(void) {
    __asm volatile("nop");
}

/* 故障 1：除以零（需先置位 SCB->CCR 的 DIV_0_TRP） */
__attribute__((noinline)) int trigger_div_zero(int numerator, int divisor) {
    int q = numerator / divisor;
    g_sink = (uint32_t)q;
    return q;
}

/* 故障 2：调用最低位为 0 的函数指针，触发 UsageFault -> INVSTATE */
__attribute__((noinline)) void trigger_invstate(void) {
    uintptr_t addr = (uintptr_t)dummy_target & ~(uintptr_t)1u;
    void (*bad_fn)(void) = (void (*)(void))addr;
    bad_fn();
}

/* 故障 3：读取未映射地址，触发 BusFault -> PRECISERR + BFARVALID */
__attribute__((noinline)) uint32_t trigger_busfault(void) {
    volatile uint32_t *bad_ptr = (volatile uint32_t *)0xCCCCCCCCu;
    uint32_t v = *bad_ptr;
    g_sink = v;
    return v;
}

int main(void) {
    clock_init_72mhz();
    console_init(72000000u, 115200u);
    fault_enable_traps();

    uart_puts("fault_demo ready: 1=div0, 2=invstate, 3=busfault\n");

    for (;;) {
        char cmd = uart_getc();
        if (cmd == '1') {
            volatile int zero = 0;
            int q = trigger_div_zero(100, zero);
            (void)q;
        } else if (cmd == '2') {
            trigger_invstate();
        } else if (cmd == '3') {
            uint32_t v = trigger_busfault();
            (void)v;
        }
    }
}
