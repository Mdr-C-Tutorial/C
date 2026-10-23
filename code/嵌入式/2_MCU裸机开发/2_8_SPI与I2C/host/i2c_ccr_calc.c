#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char *name;
    uint32_t fscl_target;
    int fast_mode;
    int duty_16_9;
    uint32_t trise_max_ns;
} i2c_mode_spec;

int main(void) {
    const uint32_t pclk1_hz = 36000000u;
    const uint32_t freq_mhz = pclk1_hz / 1000000u;
    const double tpclk1_ns = 1e9 / (double)pclk1_hz;

    const i2c_mode_spec specs[] = {
        {"Sm 100 kHz",         100000u, 0, 0, 1000u},
        {"Fm 400 kHz (DUTY=0)", 400000u, 1, 0,  300u},
        {"Fm 400 kHz (DUTY=1)", 400000u, 1, 1,  300u},
    };

    printf("PCLK1 = %u MHz (T_PCLK1 = %.2f ns), CR2.FREQ = %u (0x%02X)\n\n",
           freq_mhz, tpclk1_ns, freq_mhz, freq_mhz);
    printf("%-20s %-6s %-10s %-7s %-10s %-10s %-10s\n",
           "Mode", "CCR", "CCR_REG", "TRISE", "T_high(ns)", "T_low(ns)", "f_SCL(kHz)");

    for (size_t i = 0; i < sizeof specs / sizeof specs[0]; ++i) {
        const i2c_mode_spec *s = &specs[i];
        uint32_t coeff = 2u;
        uint32_t mult_h = 1u;
        uint32_t mult_l = 1u;

        if (s->fast_mode) {
            if (s->duty_16_9) {
                coeff = 25u;
                mult_h = 9u;
                mult_l = 16u;
            } else {
                coeff = 3u;
                mult_h = 1u;
                mult_l = 2u;
            }
        }

        uint32_t denom = coeff * s->fscl_target;
        uint32_t ccr = (pclk1_hz + denom - 1u) / denom;
        uint32_t ccr_reg = ccr;
        if (s->fast_mode) {
            ccr_reg |= (1u << 15);
            if (s->duty_16_9) {
                ccr_reg |= (1u << 14);
            }
        }

        /* 手册的算法：TRISE = 最大上升时间（以 T_PCLK1 为单位，向下取整）+ 1 */
        uint32_t trise = (s->trise_max_ns * freq_mhz) / 1000u + 1u;
        double thigh_ns = (double)(mult_h * ccr) * tpclk1_ns;
        double tlow_ns = (double)(mult_l * ccr) * tpclk1_ns;
        double fscl_khz = 1e6 / (thigh_ns + tlow_ns);

        printf("%-20s %-6u 0x%04X     %-7u %-10.1f %-10.1f %-10.2f\n",
               s->name, ccr, ccr_reg, trise, thigh_ns, tlow_ns, fscl_khz);
    }
    return 0;
}
