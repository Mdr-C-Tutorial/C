/*
 * tim_calc.c：在宿主机上验证无符号计数回绕比较、PSC/ARR 参数计算与 16 位输入捕获相减。
 */
#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char *scene;
    uint32_t f_tim_hz;
    uint16_t psc;
    uint16_t arr;
} tim_cfg_t;

static void demo_tick_wrap(void) {
    uint32_t start = 0xFFFFFFF0u;
    uint32_t interval = 20u;
    uint32_t samples[] = { 0xFFFFFFF5u, 0xFFFFFFFFu, 0x00000003u, 0x00000004u };

    printf("=== 1. SysTick 32 位毫秒计数回绕演示 (start = 0x%08X, interval = %u) ===\n",
           start, interval);
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        uint32_t now = samples[i];
        uint32_t elapsed = (uint32_t)(now - start);
        int wrong_cmp = (now >= start + interval);
        int right_cmp = (elapsed >= interval);
        printf("  now = 0x%08X | (uint32_t)(now - start) = %2u ms | "
               "now >= start + interval: %d | 差值比较: %d\n",
               now, elapsed, wrong_cmp, right_cmp);
    }
    printf("\n");
}

static void demo_tim_params(void) {
    tim_cfg_t cfgs[] = {
        { "TIM2 周期更新中断 (1 Hz)",   72000000u, 7199u,  9999u },
        { "TIM3 呼吸灯 PWM (1 kHz)",    72000000u,   71u,   999u },
        { "TIM3 舵机控制 PWM (50 Hz)",  72000000u,   71u, 19999u },
        { "TIM3 电机驱动 PWM (20 kHz)", 72000000u,    0u,  3599u },
    };

    printf("=== 2. 通用定时器 PSC / ARR 与分辨率计算 (f_TIM = 72 MHz) ===\n");
    for (size_t i = 0; i < sizeof(cfgs) / sizeof(cfgs[0]); ++i) {
        uint32_t div = (uint32_t)cfgs[i].psc + 1u;
        uint32_t steps = (uint32_t)cfgs[i].arr + 1u;
        double f_cnt = (double)cfgs[i].f_tim_hz / (double)div;
        double f_out = f_cnt / (double)steps;
        double step_us = 1e6 / f_cnt;
        printf("  %-28s: PSC = %4u, ARR = %5u | f_CNT = %8.0f Hz (步进 %6.3f us) | "
               "f = %7.1f Hz, 共 %5u 级\n",
               cfgs[i].scene, cfgs[i].psc, cfgs[i].arr, f_cnt, step_us, f_out, steps);
    }
    printf("\n");
}

static void demo_ic_wrap(void) {
    uint16_t cap1 = 65000u;
    uint16_t cap2 = 464u; /* 65000 + 1000 = 66000，模 65536 余 464 */
    uint16_t diff = (uint16_t)(cap2 - cap1);

    printf("=== 3. 16 位输入捕获跨 0xFFFF 回绕相减 ===\n");
    printf("  上次捕获 cap1 = %u (0x%04X)，本次捕获 cap2 = %u (0x%04X)\n",
           cap1, cap1, cap2, cap2);
    printf("  (uint16_t)(cap2 - cap1) = %u 步 = %u us -> 实测频率 %u Hz\n",
           diff, diff, 1000000u / diff);
}

int main(void) {
    demo_tick_wrap();
    demo_tim_params();
    demo_ic_wrap();
    return 0;
}
