/*
 * 展示 gprof 的盲区：全部计算时间都花在动态库里的 sprintf 上。
 *
 *   gcc -std=c17 -O2 -pg -no-pie loop.c -o loop
 *   ./loop
 *   gprof -b loop gmon.out
 *
 * 墙钟时间两百多毫秒，gprof 却报告 "no time accumulated"，
 * 因为 sprintf 在 UCRT 动态库中，没有 -pg 插桩。
 */
#include <stdio.h>
#include <string.h>

static char buffer[64];

int main(void) {
    volatile int seed = 1;
    for (int i = 0; i < 4000000; i++) {
        int n = sprintf(buffer, "%d-%s", seed + i, "some fairly long text");
        seed = buffer[0] + n;
    }
    printf("%d\n", seed);
    return 0;
}