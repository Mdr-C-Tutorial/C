#define _POSIX_C_SOURCE 200809L

/* 相对睡眠与绝对睡眠的漂移对比。
 *
 *   ./drift       每轮工作 7 ms，然后 nanosleep(100 ms)
 *   ./drift abs   每轮工作 7 ms，然后 clock_nanosleep 到第 i 个截止时刻
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define ROUNDS 10
#define PERIOD_MS 100
#define WORK_MS 7

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void sleep_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
    }
}

static void busy_ms(long ms) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000L +
                       (now.tv_nsec - start.tv_nsec) / 1000000L;
        if (elapsed >= ms) {
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    int absolute = argc > 1 && argv[1][0] == 'a';

    struct timespec start;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        die("clock_gettime");
    }

    for (int i = 0; i < ROUNDS; i++) {
        busy_ms(WORK_MS);

        if (absolute) {
            struct timespec deadline = start;
            deadline.tv_nsec += (long)(i + 1) * PERIOD_MS * 1000000L;
            deadline.tv_sec += deadline.tv_nsec / 1000000000L;
            deadline.tv_nsec %= 1000000000L;
            while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline,
                                   NULL) == EINTR) {
            }
        } else {
            sleep_ms(PERIOD_MS);
        }

        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
            die("clock_gettime");
        }
        long long elapsed_us =
            (long long)(now.tv_sec - start.tv_sec) * 1000000LL +
            (now.tv_nsec - start.tv_nsec) / 1000;
        printf("round %2d: %8.3f ms (ideal %4d ms)\n", i + 1,
               (double)elapsed_us / 1000.0, (i + 1) * PERIOD_MS);
    }
    return 0;
}