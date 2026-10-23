#define _POSIX_C_SOURCE 200809L

/* 三种时钟测同一段 700 ms 的睡眠，并打印单调时钟的分辨率。 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static double now_ms(clockid_t clock) {
    struct timespec ts;
    if (clock_gettime(clock, &ts) == -1) {
        die("clock_gettime");
    }
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(void) {
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) == -1) {
        die("clock_getres");
    }
    printf("CLOCK_MONOTONIC resolution: %ld ns\n",
           (long)res.tv_sec * 1000000000L + res.tv_nsec);

    double real_start = now_ms(CLOCK_REALTIME);
    double mono_start = now_ms(CLOCK_MONOTONIC);
    double boot_start = now_ms(CLOCK_BOOTTIME);

    struct timespec work = {0, 700 * 1000 * 1000};
    nanosleep(&work, NULL);

    printf("sleep 700 ms measured with CLOCK_REALTIME:  %.3f ms\n",
           now_ms(CLOCK_REALTIME) - real_start);
    printf("sleep 700 ms measured with CLOCK_MONOTONIC: %.3f ms\n",
           now_ms(CLOCK_MONOTONIC) - mono_start);
    printf("sleep 700 ms measured with CLOCK_BOOTTIME:  %.3f ms\n",
           now_ms(CLOCK_BOOTTIME) - boot_start);

    struct timespec mono;
    clock_gettime(CLOCK_MONOTONIC, &mono);
    printf("CLOCK_MONOTONIC now: %ld.%09ld\n", (long)mono.tv_sec, mono.tv_nsec);
    return 0;
}