/*
 * race_disasm.c：演示 volatile 共享计数器的自增在 Cortex-M3 上的指令序列。
 *
 * 编译与反汇编：
 *   arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra -c race_disasm.c
 *   arm-none-eabi-objdump -d race_disasm.o
 */
#include <stdint.h>

volatile uint32_t g_count;

void count_inc_unprotected(void) {
    g_count++;
}
