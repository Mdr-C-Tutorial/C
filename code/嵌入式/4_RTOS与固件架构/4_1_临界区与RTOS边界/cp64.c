/*
 * cp64.c：在 PRIMASK 临界区里复制 64 字节，用来估算临界区的最长持续时间。
 *
 * 编译与反汇编：
 *   arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra \
 *       -DSTM32F103xB -I<cmsis 目录> -c cp64.c
 *   arm-none-eabi-objdump -d cp64.o
 */
#include <stdint.h>
#include "stm32f1xx.h"

void log_copy(uint32_t *dst, const uint32_t *src) {
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    for (int i = 0; i < 16; ++i) {
        dst[i] = src[i];
    }
    __set_PRIMASK(saved);
}
