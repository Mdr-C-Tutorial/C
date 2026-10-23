/*
 * adc_calc.c：在宿主机（MinGW / Linux gcc）上验证 ADC 转换时间表、
 * VREFINT 修正电压换算、片内温度换算与 PWM+RC 低通滤波参数。
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* 直接按标称 3300 mV 换算 */
static uint32_t adc_to_mv_nominal(uint16_t raw) {
    return ((uint32_t)raw * 3300u + 2047u) / 4095u;
}

/* 利用内部参考电压通道 IN17（典型值 1200 mV）反推实际 VDDA（单位 mV） */
static uint32_t adc_vdda_mv(uint16_t raw_vref) {
    return (1200u * 4095u + (raw_vref / 2u)) / raw_vref;
}

/* 利用实测 VDDA 换算目标通道电压（单位 mV） */
static uint32_t adc_to_mv_calibrated(uint16_t raw_ch, uint32_t vdda_mv) {
    return ((uint32_t)raw_ch * vdda_mv + 2047u) / 4095u;
}

/*
 * 换算片内温度传感器读数（返回 0.1 摄氏度为单位的整数，如 284 表示 28.4 C）。
 * DS5319 表 51 典型参数：V25 = 1.43 V（1430 mV），Avg_Slope = 4.3 mV/C。
 */
static int32_t adc_temp_decic(uint16_t raw_temp, uint32_t vdda_mv) {
    uint32_t vsense_mv = ((uint32_t)raw_temp * vdda_mv + 2047u) / 4095u;
    return ((1430 - (int32_t)vsense_mv) * 100) / 43 + 250;
}

int main(void) {
    static const double smp_cycles[8] = {
        1.5, 7.5, 13.5, 28.5, 41.5, 55.5, 71.5, 239.5
    };

    printf("=== 1. ADC conversion time table (ADCCLK = 12 MHz vs 14 MHz) ===\n");
    printf("SMP  Cycles   t_conv@12MHz   f_s@12MHz     t_conv@14MHz   f_s@14MHz\n");
    for (int i = 0; i < 8; ++i) {
        double total_cyc = smp_cycles[i] + 12.5;
        double t12_us = total_cyc / 12.0;
        double fs12_khz = 12000.0 / total_cyc;
        double t14_us = total_cyc / 14.0;
        double fs14_khz = 14000.0 / total_cyc;
        printf(" %d   %5.1f    %6.2f us     %6.1f kS/s    %6.2f us     %6.1f kS/s\n",
               i, smp_cycles[i], t12_us, fs12_khz, t14_us, fs14_khz);
    }

    printf("\n=== 2. Voltage & temperature conversion with VREFINT calibration ===\n");
    struct sample_case {
        const char *label;
        uint16_t raw_pa1;
        uint16_t raw_temp;
        uint16_t raw_vref;
    };
    static const struct sample_case cases[] = {
        {"LDO 3.30 V", 2048u, 1756u, 1489u},
        {"USB 3.18 V", 2125u, 1822u, 1545u},
        {"Bat 3.00 V", 2252u, 1932u, 1638u},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        uint32_t nom_mv = adc_to_mv_nominal(cases[i].raw_pa1);
        uint32_t vdda = adc_vdda_mv(cases[i].raw_vref);
        uint32_t cal_mv = adc_to_mv_calibrated(cases[i].raw_pa1, vdda);
        int32_t t_decic = adc_temp_decic(cases[i].raw_temp, vdda);
        printf("[%s] raw_pa1=%4u raw_vref=%4u -> nominal=%4lu mV, "
               "VDDA=%4lu mV, calibrated=%4lu mV, T=%ld.%ld C\n",
               cases[i].label,
               (unsigned)cases[i].raw_pa1,
               (unsigned)cases[i].raw_vref,
               (unsigned long)nom_mv,
               (unsigned long)vdda,
               (unsigned long)cal_mv,
               (long)(t_decic / 10),
               (long)(t_decic >= 0 ? t_decic % 10 : -(t_decic % 10)));
    }

    printf("\n=== 3. PWM + RC low-pass filter (f_PWM = 20 kHz, VDDA = 3.3 V) ===\n");
    const double pi = 3.14159265358979323846;
    const double f_pwm = 20000.0;
    struct rc_choice {
        double r_ohm;
        double c_farad;
    };
    static const struct rc_choice rcs[] = {
        {10000.0, 100e-9},
        {10000.0, 1e-6},
        {4700.0,  1e-6},
    };
    for (size_t i = 0; i < sizeof rcs / sizeof rcs[0]; ++i) {
        double tau_ms = rcs[i].r_ohm * rcs[i].c_farad * 1000.0;
        double fc = 1.0 / (2.0 * pi * rcs[i].r_ohm * rcs[i].c_farad);
        double ripple_mv = (3300.0 * 0.5 * 0.5) / (f_pwm * rcs[i].r_ohm * rcs[i].c_farad);
        printf("R=%5.1f kOhm, C=%5.2f uF -> fc=%6.2f Hz, tau=%5.2f ms, "
               "50%% duty ripple ~= %4.2f mVpp\n",
               rcs[i].r_ohm / 1000.0,
               rcs[i].c_farad * 1e6,
               fc, tau_ms, ripple_mv);
    }
    return 0;
}
