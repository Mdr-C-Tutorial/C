#include "fault.h"
#include "console.h"
#include "stm32f1xx.h"

__attribute__((section(".noinit"))) static crash_record_t g_crash_record;

static uint32_t crash_checksum(const crash_record_t *rec) {
    return rec->magic ^ rec->pc ^ rec->lr ^ rec->cfsr ^ rec->hfsr ^ rec->bfar ^
           0xA5A55A5Au;
}

static void uart_puthex32(uint32_t val) {
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(val >> shift) & 0xFu]);
    }
}

static void uart_print_reg(const char *name, uint32_t val) {
    uart_puts(name);
    uart_puthex32(val);
    uart_puts("\n");
}

void fault_enable_traps(void) {
    /* 捕获除零错误；若需捕获非对齐访问，可同时置位 SCB_CCR_UNALIGN_TRP_Msk */
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
}

int crash_record_load(crash_record_t *out) {
    if (g_crash_record.magic != CRASH_MAGIC) {
        return 0;
    }
    if (g_crash_record.checksum != crash_checksum(&g_crash_record)) {
        return 0;
    }
    *out = g_crash_record;
    return 1;
}

void crash_record_clear(void) {
    g_crash_record.magic = 0;
    g_crash_record.checksum = 0;
}

void hardfault_c(const fault_frame_t *frame) {
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t bfar = SCB->BFAR;

    g_crash_record.magic = CRASH_MAGIC;
    g_crash_record.pc = frame->pc;
    g_crash_record.lr = frame->lr;
    g_crash_record.cfsr = cfsr;
    g_crash_record.hfsr = hfsr;
    g_crash_record.bfar = bfar;
    g_crash_record.checksum = crash_checksum(&g_crash_record);

    uart_puts("\n=== HardFault ===\n");
    uart_print_reg("R0   = ", frame->r0);
    uart_print_reg("R1   = ", frame->r1);
    uart_print_reg("R2   = ", frame->r2);
    uart_print_reg("R3   = ", frame->r3);
    uart_print_reg("R12  = ", frame->r12);
    uart_print_reg("LR   = ", frame->lr);
    uart_print_reg("PC   = ", frame->pc);
    uart_print_reg("xPSR = ", frame->xpsr);
    uart_print_reg("CFSR = ", cfsr);
    uart_print_reg("HFSR = ", hfsr);
    if ((cfsr & SCB_CFSR_BFARVALID_Msk) != 0) {
        uart_print_reg("BFAR = ", bfar);
    }
    uart_flush();

    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0) {
        __BKPT(0);
    }
    NVIC_SystemReset();
}

__attribute__((naked)) void HardFault_Handler(void) {
    __asm volatile(
        "tst lr, #4      \n"
        "ite eq          \n"
        "mrseq r0, msp   \n"
        "mrsne r0, psp   \n"
        "b hardfault_c   \n"
    );
}
