#include <stdint.h>
#include <stdio.h>

static void print_brr_row(const char *bus, uint32_t f_ck, uint32_t target_baud, uint32_t brr) {
    uint32_t mantissa = brr >> 4;
    uint32_t fraction = brr & 0xFu;
    double usartdiv = (double)f_ck / (16.0 * (double)target_baud);
    double actual_baud = (double)f_ck / (double)brr;
    double err_pct = (actual_baud - (double)target_baud) / (double)target_baud * 100.0;

    printf("%-6s %9u %8u %10.4f   0x%04X (%3u, %2u) %10.1f %+7.2f%%\n",
           bus, f_ck, target_baud, usartdiv,
           brr, mantissa, fraction, actual_baud, err_pct);
}

int main(void) {
    const uint32_t bauds[] = { 9600u, 115200u, 921600u };

    printf("Bus     f_CK(Hz)   Target   USARTDIV   BRR (Mant, Frac)     Actual    Error\n");
    printf("------ --------- -------- ---------- ------------------ ---------- --------\n");

    for (size_t i = 0; i < sizeof bauds / sizeof bauds[0]; ++i) {
        uint32_t b = bauds[i];
        uint32_t brr = (72000000u + b / 2u) / b;
        print_brr_row("PCLK2", 72000000u, b, brr);
    }

    for (size_t i = 0; i < sizeof bauds / sizeof bauds[0]; ++i) {
        uint32_t b = bauds[i];
        uint32_t brr = (36000000u + b / 2u) / b;
        print_brr_row("PCLK1", 36000000u, b, brr);
    }

    /* RM0008 表 192 中 36 MHz / 115200 取 19.5（BRR = 0x0138）的对照行 */
    print_brr_row("PCLK1*", 36000000u, 115200u, 0x0138u);
    return 0;
}
