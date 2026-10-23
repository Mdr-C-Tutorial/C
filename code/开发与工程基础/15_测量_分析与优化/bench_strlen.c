/*
 * 微基准的两个版本：同一段循环，两种写法。
 *
 *   bench_strlen TEXT
 *
 * 第一版把 strlen 的返回值累加进普通局部对象。它看起来没问题，但 -O2
 * 发现每次循环做的都是同一件事，于是把循环折叠成一次 strlen 再乘上次数。
 * 第二版累加进 volatile 对象，编译器必须让每次读写真的发生。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -O2 bench_strlen.c -o bench_strlen
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

#define ITERATIONS 10000000L

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s TEXT\n", argv[0]);
        return 2;
    }
    const char *text = argv[1];

    /* 第一版：结果累加进普通局部对象 */
    size_t total = 0;
    double start = now_seconds();
    for (long i = 0; i < ITERATIONS; i++) {
        total += strlen(text);
    }
    double end = now_seconds();
    printf("plain local   %6.3f s  (total = %zu)\n", end - start, total);

    /* 第二版：结果累加进 volatile 对象 */
    volatile size_t sink = 0;
    start = now_seconds();
    for (long i = 0; i < ITERATIONS; i++) {
        sink += strlen(text);
    }
    end = now_seconds();
    printf("volatile sink %6.3f s  (sink = %zu)\n", end - start, (size_t)sink);
    return 0;
}