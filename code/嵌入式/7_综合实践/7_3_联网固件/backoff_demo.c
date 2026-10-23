/*
 * backoff_demo.c：固定间隔重连与指数退避加抖动的对比。
 *
 * 抖动取 backoffAlgorithm 的做法：先算出第 n 次尝试的上限 ceiling（从
 * base 开始逐次翻倍，封顶 max），再在 ceiling 与 ceiling / 2 之间取一个
 * 随机值。这里用线性同余发生器代替 rand()，输出才能复现。
 *
 * 编译：gcc -std=c17 -Wall -Wextra backoff_demo.c -o backoff_demo
 */
#include <stdint.h>
#include <stdio.h>

#define BASE_MS 1000u
#define MAX_MS 64000u
#define ATTEMPTS 8u

struct backoff {
    uint32_t base_ms;
    uint32_t max_ms;
    uint32_t ceiling;
    uint32_t seed;
};

/* 固定间隔：每次都是 1 秒，退避状态形同虚设 */
static uint32_t fixed_next(void) {
    return BASE_MS;
}

/* 指数退避加抖动：先翻倍上限，再在 [ceiling/2, ceiling] 里取随机值 */
static uint32_t backoff_next(struct backoff *bo) {
    if (bo->ceiling < bo->max_ms) {
        bo->ceiling *= 2u;
        if (bo->ceiling > bo->max_ms) {
            bo->ceiling = bo->max_ms;
        }
    }

    bo->seed = bo->seed * 1664525u + 1013904223u;
    uint32_t span = bo->ceiling / 2u;
    return bo->ceiling - (span == 0u ? 0u : (bo->seed >> 8) % (span + 1u));
}

int main(void) {
    printf("固定间隔（8 次尝试，单位秒）：\n  ");
    uint32_t total = 0;
    for (uint32_t i = 0; i < ATTEMPTS; ++i) {
        uint32_t ms = fixed_next();
        printf("%5.1f", (double)ms / 1000.0);
        total += ms;
    }
    printf("\n  累计 %u 秒\n\n", (unsigned)(total / 1000u));

    printf("指数退避加抖动（3 台设备，各 8 次尝试，单位秒）：\n");
    for (uint32_t dev = 1; dev <= 3; ++dev) {
        struct backoff bo = {
            .base_ms = BASE_MS,
            .max_ms = MAX_MS,
            .ceiling = BASE_MS / 2u, /* 第一次调用翻倍，上限回到 1 秒 */
            .seed = 20261021u + dev * 7919u,
        };
        uint32_t sum = 0;
        printf("  设备 %u：", (unsigned)dev);
        for (uint32_t i = 0; i < ATTEMPTS; ++i) {
            uint32_t ms = backoff_next(&bo);
            printf("%5.1f", (double)ms / 1000.0);
            sum += ms;
        }
        printf("\n          累计 %.1f 秒\n", (double)sum / 1000.0);
    }

    printf("\n上限序列：1、2、4、8、16、32、64、64 秒，之后一直封顶在 %u 秒\n",
           (unsigned)(MAX_MS / 1000u));
    return 0;
}