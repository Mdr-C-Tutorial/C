/*
 * spi_br_calc.c：列出 SPI1 在 PCLK2 = 72 MHz 下的全部分频，标出超过
 * STM32F103 数据手册 18 Mbit/s 上限的档位，并估算一次读 JEDEC ID 的时间。
 */
#include <stdint.h>
#include <stdio.h>

int main(void) {
    const uint32_t pclk2_hz = 72000000u;
    const uint32_t limit_hz = 18000000u;
    static const uint32_t divider[8] = {2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u};

    printf("SPI1 on APB2, PCLK2 = %lu Hz, datasheet limit = %lu bit/s\n",
           (unsigned long)pclk2_hz, (unsigned long)limit_hz);
    printf("%-8s %-8s %-12s %-10s %s\n", "BR[2:0]", "分频", "f_SCK", "周期(ns)", "是否可用");

    for (uint32_t br = 0; br < 8u; ++br) {
        uint32_t fsck = pclk2_hz / divider[br];
        printf("%03lu      /%-7lu %-12lu %-10lu %s\n", (unsigned long)br,
               (unsigned long)divider[br], (unsigned long)fsck,
               (unsigned long)(1000000000u / fsck),
               fsck <= limit_hz ? "可用" : "超出手册上限");
    }

    /* 读 JEDEC ID：命令 1 字节 + 3 字节返回 = 32 个 SCK，另加 CS 建立与保持时间 */
    const uint32_t bits = 32u;
    printf("\nJEDEC ID 一次 32 个时钟，18 MHz 下约 %.2f us\n",
           (double)bits * 1e6 / 18000000.0);
    return 0;
}