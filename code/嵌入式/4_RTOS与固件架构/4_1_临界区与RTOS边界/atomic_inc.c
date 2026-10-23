/*
 * atomic_inc.c：C11 原子操作在 Cortex-M3 上生成的 LDREX/STREX 重试循环。
 *
 * 编译与反汇编：
 *   arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra -c atomic_inc.c
 *   arm-none-eabi-objdump -d atomic_inc.o
 */
#include <stdatomic.h>
#include <stdint.h>

_Static_assert(ATOMIC_INT_LOCK_FREE == 2,
               "32-bit atomic integers must be always lock-free on Cortex-M3");

atomic_uint_least32_t g_tx_bytes;

void add_tx_bytes(uint32_t n) {
    atomic_fetch_add_explicit(&g_tx_bytes, n, memory_order_relaxed);
}
