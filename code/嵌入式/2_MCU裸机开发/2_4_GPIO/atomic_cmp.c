/*
 * atomic_cmp.c：对比 BSRR 写入与 ODR 读-改-写编译生成的 Thumb-2 机器指令。
 *
 * 编译与反汇编：
 *   arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra \
 *       -ffreestanding -DSTM32F103xB -c atomic_cmp.c -o atomic_cmp.o
 *   arm-none-eabi-objdump -d atomic_cmp.o
 */
#include <stdint.h>
#include "stm32f1xx.h"

void pc13_low_bsrr(void) {
    GPIOC->BSRR = GPIO_BSRR_BR13;
}

void pc13_toggle_odr(void) {
    GPIOC->ODR ^= GPIO_ODR_ODR13;
}
