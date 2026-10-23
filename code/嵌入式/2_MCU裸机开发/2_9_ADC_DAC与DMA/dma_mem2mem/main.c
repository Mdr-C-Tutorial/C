/*
 * dma_mem2mem/main.c：用 DMA1 通道 1 的内存到内存（MEM2MEM）模式复制数组。
 */
#include "clock.h"
#include "console.h"
#include "stm32f1xx.h"
#include "tick.h"
#include <stdint.h>
#include <stdio.h>

#define WORD_COUNT 16u

static const uint32_t g_src[WORD_COUNT] = {
    0x11110000u, 0x11110001u, 0x11110002u, 0x11110003u,
    0x22220004u, 0x22220005u, 0x22220006u, 0x22220007u,
    0x33330008u, 0x33330009u, 0x3333000Au, 0x3333000Bu,
    0x4444000Cu, 0x4444000Du, 0x4444000Eu, 0x4444000Fu,
};

/* DMA 后台写入的目标缓冲区，声明为 volatile 防止编译器假设其未被修改 */
static volatile uint32_t g_dst[WORD_COUNT];

static void dma1_mem2mem_copy(volatile uint32_t *dst,
                              const uint32_t *src,
                              uint16_t count) {
    /* 1. 开启 AHB 总线上的 DMA1 时钟 */
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    /* 2. 先关闭通道（EN = 0），才能写入 CPAR、CMAR、CNDTR 和 CCR */
    DMA1_Channel1->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF1;

    /* 3. DIR = 0 时从 CPAR 读、往 CMAR 写 */
    DMA1_Channel1->CPAR = (uint32_t)(uintptr_t)src;
    DMA1_Channel1->CMAR = (uint32_t)(uintptr_t)dst;
    DMA1_Channel1->CNDTR = count;

    /*
     * 4. 配置并启动通道：
     *    - MEM2MEM = 1：内存到内存模式，无需外设请求，EN 置位即刻开始搬运
     *    - PSIZE = 10（32 位），MSIZE = 10（32 位）
     *    - PINC = 1，MINC = 1：源地址与目标地址每次递增 4 字节
     *    - DIR = 0：从 CPAR 读入，写向 CMAR
     */
    DMA1_Channel1->CCR = DMA_CCR_MEM2MEM |
                         DMA_CCR_PSIZE_1 |
                         DMA_CCR_MSIZE_1 |
                         DMA_CCR_PINC |
                         DMA_CCR_MINC |
                         DMA_CCR_EN;

    /* 5. 等待传输完成标志 TCIF1 置位，随后清除标志并关闭通道 */
    while ((DMA1->ISR & DMA_ISR_TCIF1) == 0) {
    }
    DMA1->IFCR = DMA_IFCR_CGIF1;
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);

    dma1_mem2mem_copy(g_dst, g_src, WORD_COUNT);

    uint32_t mismatches = 0u;
    for (uint32_t i = 0u; i < WORD_COUNT; ++i) {
        if (g_dst[i] != g_src[i]) {
            ++mismatches;
        }
    }
    printf("DMA MEM2MEM copy %u words done, mismatches = %lu\n",
           WORD_COUNT, (unsigned long)mismatches);

    for (;;) {
        delay_ms(1000);
    }
}
