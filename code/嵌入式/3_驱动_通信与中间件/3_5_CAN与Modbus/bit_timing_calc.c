#include <stdint.h>
#include <stdio.h>

static void print_bit_timing_candidates(uint32_t pclk1_hz, uint32_t bitrate) {
    printf("=== PCLK1 = %u Hz, target = %u bit/s ===\n", pclk1_hz, bitrate);
    printf("%-6s %-6s %-6s %-8s %-10s %-12s\n",
           "Presc", "BS1", "BS2", "TotalTq", "SamplePt", "CAN_BTR(SJW=1)");

    for (uint32_t total_tq = 8u; total_tq <= 25u; ++total_tq) {
        uint32_t prod = bitrate * total_tq;
        if (pclk1_hz % prod != 0u) {
            continue;
        }
        uint32_t presc = pclk1_hz / prod;
        if (presc < 1u || presc > 1024u) {
            continue;
        }

        for (uint32_t bs1 = 1u; bs1 <= 16u; ++bs1) {
            if (1u + bs1 >= total_tq) {
                continue;
            }
            uint32_t bs2 = total_tq - 1u - bs1;
            if (bs2 < 1u || bs2 > 8u) {
                continue;
            }

            /* 采样点比例 = (1 + BS1) / (1 + BS1 + BS2)，筛选 85% ~ 90% 的常用配置 */
            double sp = (double)(1u + bs1) * 100.0 / (double)total_tq;
            if (sp < 85.0 || sp > 90.0) {
                continue;
            }

            uint32_t brp = presc - 1u;
            uint32_t ts1 = bs1 - 1u;
            uint32_t ts2 = bs2 - 1u;
            uint32_t sjw = 0u; /* SJW = 1 tq */
            uint32_t btr = (sjw << 24) | (ts2 << 20) | (ts1 << 16) | brp;

            printf("%-6u %-6u %-6u %-8u %6.1f%%    0x%08X\n",
                   presc, bs1, bs2, total_tq, sp, btr);
        }
    }
}

int main(void) {
    const uint32_t pclk1 = 36000000u;
    print_bit_timing_candidates(pclk1, 500000u);
    printf("\n");
    print_bit_timing_candidates(pclk1, 250000u);
    printf("\n");
    print_bit_timing_candidates(pclk1, 125000u);
    return 0;
}
