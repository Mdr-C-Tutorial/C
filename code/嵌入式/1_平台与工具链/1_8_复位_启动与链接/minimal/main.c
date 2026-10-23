/*
 * main.c：1.8 节最小固件示例。
 *
 * 不访问外设，只定义几类静态存储期对象，用来观察编译链接后 .text、.rodata、
 * .data、.bss 在 Flash 和 SRAM 中的分布。
 */
#include <stdint.h>

/* 只读数据：放进 .rodata 段，留在 Flash 中 */
const uint32_t g_scale_table[4] = {10, 20, 30, 40};

/* 有初值的可写静态存储期对象：初值存放在 Flash（LMA），运行时位于 SRAM（VMA） */
uint32_t g_weights[4] = {1, 2, 3, 4};

/* 无初值的可写静态存储期对象：放进 .bss 段，由 Reset_Handler 清零 */
volatile uint32_t g_tick_count;
uint32_t g_accum[4];

uint32_t step_once(uint32_t index) {
    uint32_t i = index & 3u;
    uint32_t val = g_weights[i] * g_scale_table[i];
    g_accum[i] += val;
    g_weights[i] += 1u;
    g_tick_count += 1u;
    return g_accum[i];
}

int main(void) {
    uint32_t i = 0;
    for (;;) {
        (void)step_once(i);
        ++i;
    }
}
