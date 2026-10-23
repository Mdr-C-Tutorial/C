#include "clock.h"
#include "console.h"
#include "fault.h"
#include "stm32f1xx.h"
#include "tick.h"

static void uart_puthex32(uint32_t val) {
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(val >> shift) & 0xFu]);
    }
}

static void iwdg_init_1s(void) {
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;
    IWDG->KR = 0x5555u;
    IWDG->PR = 4u;
    IWDG->RLR = 624u;
    IWDG->KR = 0xAAAAu;
    IWDG->KR = 0xCCCCu;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);
    fault_enable_traps();

    reset_cause_t cause = reset_cause_get();
    uart_puts("boot: reset cause = ");
    uart_puts(reset_cause_name(cause));
    uart_puts("\n");

    crash_record_t rec;
    if (crash_record_load(&rec)) {
        uart_puts("last crash record in .noinit:\n  PC   = ");
        uart_puthex32(rec.pc);
        uart_puts("\n  LR   = ");
        uart_puthex32(rec.lr);
        uart_puts("\n  CFSR = ");
        uart_puthex32(rec.cfsr);
        uart_puts("\n  HFSR = ");
        uart_puthex32(rec.hfsr);
        uart_puts("\n  BFAR = ");
        uart_puthex32(rec.bfar);
        uart_puts("\n");
        crash_record_clear();
    }

    iwdg_init_1s();

    uint32_t last_feed = tick_now();
    for (;;) {
        if (tick_elapsed(last_feed, 250u)) {
            last_feed = tick_now();
            IWDG->KR = 0xAAAAu;
        }
    }
}
