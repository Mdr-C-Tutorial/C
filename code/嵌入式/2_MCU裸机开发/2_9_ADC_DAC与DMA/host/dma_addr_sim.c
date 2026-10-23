/*
 * dma_addr_sim.c：在主机上模拟 DMA 通道的地址生成，观察 MINC 的作用。
 *
 * 用 g_adc_dr 代替 ADC_DR：ADC 每完成一次转换就把结果写进这个固定地址，
 * DMA 每次从这里读；内存一侧用数组，MINC 决定写指针是否前进。
 * 打印的地址一律写成相对缓冲区首地址的偏移，避免每次运行的绝对地址不同。
 */
#include <stdint.h>
#include <stdio.h>

#define ITEMS 3u

static volatile uint16_t g_adc_dr;

static void run_transfer(const char *label, int minc, const uint16_t *samples,
                         uint16_t *dst, uint32_t items) {
    uintptr_t dst_base = (uintptr_t)dst;
    uintptr_t dst_addr = dst_base;

    printf("== %s ==\n", label);
    for (uint32_t i = 0u; i < items; ++i) {
        /* 模拟一次转换：ADC 把新结果写进同一个 DR */
        g_adc_dr = samples[i];

        /* DMA 每次从固定地址 DR 读 16 位，写进内存 */
        uint16_t value = g_adc_dr;
        *(uint16_t *)dst_addr = value;
        printf("  第 %lu 次：读 DR+0 -> 写 buf+%lu，值 0x%04X\n",
               (unsigned long)(i + 1u),
               (unsigned long)(dst_addr - dst_base), (unsigned)value);

        if (minc != 0) {
            dst_addr += 2u;
        }
    }

    printf("  缓冲区：");
    for (uint32_t i = 0u; i < items; ++i) {
        printf(" %04X", dst[i]);
    }
    printf("\n\n");
}

int main(void) {
    /* 模拟 ADC 连续三次转换的结果 */
    static const uint16_t samples[ITEMS] = {0x0A11u, 0x0B22u, 0x0C33u};
    uint16_t buf_minc[ITEMS] = {0u, 0u, 0u};
    uint16_t buf_nominc[ITEMS] = {0u, 0u, 0u};

    run_transfer("MINC = 1，PSIZE = MSIZE = 16 位", 1, samples, buf_minc, ITEMS);
    run_transfer("MINC = 0，PSIZE = MSIZE = 16 位", 0, samples, buf_nominc, ITEMS);

    printf("MINC = 0 时三次写入都落在 buf+0，后一次覆盖前一次，"
           "buf[1]、buf[2] 保持初值。\n");
    printf("源地址始终是 DR：ADC 的 DR 只有一个，每次转换后硬件把新值放进去，"
           "所以 PINC 必须为 0。\n");
    return 0;
}