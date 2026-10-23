#include <stdint.h>
#include <stdio.h>

static void print_iwdg_table(void) {
    const double f_lsi = 40000.0; /* 典型值 40 kHz */
    printf("=== IWDG timeout table (f_LSI = 40 kHz) ===\n");
    printf("PR  Div   Min(RLR=0)   Max(RLR=4095)  RLR for 1.0 s\n");
    for (unsigned pr = 0; pr <= 6; ++pr) {
        unsigned div = 4u << pr;
        double t_min_ms = (double)div * 1.0 * 1000.0 / f_lsi;
        double t_max_ms = (double)div * 4096.0 * 1000.0 / f_lsi;
        double rlr_1s = (1.0 * f_lsi) / (double)div - 1.0;
        if (rlr_1s >= 0.0 && rlr_1s <= 4095.0) {
            printf("%u   %-4u  %6.2f ms    %8.1f ms     %4.0f\n",
                   pr, div, t_min_ms, t_max_ms, rlr_1s);
        } else {
            printf("%u   %-4u  %6.2f ms    %8.1f ms        -\n",
                   pr, div, t_min_ms, t_max_ms);
        }
    }
    printf("\n");
}

static void print_wwdg_example(void) {
    const double pclk1 = 36000000.0;
    unsigned wdgtb = 3u; /* 2^3 = 8 */
    unsigned t_init = 0x7Fu;
    unsigned w_val = 0x5Fu;
    double step_ms = 4096.0 * (double)(1u << wdgtb) * 1000.0 / pclk1;
    double t_min_win = step_ms * (double)(t_init - w_val);
    double t_max_out = step_ms * (double)((t_init & 0x3Fu) + 1u);
    printf("=== WWDG window example (PCLK1 = 36 MHz, WDGTB = 3, T = 0x7F, W = 0x5F) ===\n");
    printf("step = %.3f ms, refresh window = (%.2f ms, %.2f ms)\n\n",
           step_ms, t_min_win, t_max_out);
}

static void decode_cfsr(uint32_t cfsr, uint32_t hfsr) {
    printf("CFSR = 0x%08X, HFSR = 0x%08X ->", cfsr, hfsr);
    if ((hfsr & (1u << 30)) != 0) {
        printf(" [HFSR:FORCED]");
    }
    if ((hfsr & (1u << 1)) != 0) {
        printf(" [HFSR:VECTTBL]");
    }
    /* UFSR: bits [31:16] */
    if ((cfsr & (1u << 25)) != 0) {
        printf(" [UFSR:DIVBYZERO]");
    }
    if ((cfsr & (1u << 24)) != 0) {
        printf(" [UFSR:UNALIGNED]");
    }
    if ((cfsr & (1u << 19)) != 0) {
        printf(" [UFSR:NOCP]");
    }
    if ((cfsr & (1u << 18)) != 0) {
        printf(" [UFSR:INVPC]");
    }
    if ((cfsr & (1u << 17)) != 0) {
        printf(" [UFSR:INVSTATE]");
    }
    if ((cfsr & (1u << 16)) != 0) {
        printf(" [UFSR:UNDEFINSTR]");
    }
    /* BFSR: bits [15:8] */
    if ((cfsr & (1u << 15)) != 0) {
        printf(" [BFSR:BFARVALID]");
    }
    if ((cfsr & (1u << 12)) != 0) {
        printf(" [BFSR:STKERR]");
    }
    if ((cfsr & (1u << 11)) != 0) {
        printf(" [BFSR:UNSTKERR]");
    }
    if ((cfsr & (1u << 10)) != 0) {
        printf(" [BFSR:IMPRECISERR]");
    }
    if ((cfsr & (1u << 9)) != 0) {
        printf(" [BFSR:PRECISERR]");
    }
    if ((cfsr & (1u << 8)) != 0) {
        printf(" [BFSR:IBUSERR]");
    }
    /* MMFSR: bits [7:0] */
    if ((cfsr & (1u << 7)) != 0) {
        printf(" [MMFSR:MMARVALID]");
    }
    if ((cfsr & (1u << 1)) != 0) {
        printf(" [MMFSR:DACCVIOL]");
    }
    if ((cfsr & (1u << 0)) != 0) {
        printf(" [MMFSR:IACCVIOL]");
    }
    printf("\n");
}

int main(void) {
    print_iwdg_table();
    print_wwdg_example();

    printf("=== CFSR / HFSR decode examples ===\n");
    decode_cfsr(0x02000000u, 0x40000000u);
    decode_cfsr(0x00020000u, 0x40000000u);
    decode_cfsr(0x00008200u, 0x40000000u);
    decode_cfsr(0x00000400u, 0x40000000u);
    return 0;
}
