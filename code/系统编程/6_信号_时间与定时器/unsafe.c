#define _POSIX_C_SOURCE 200809L

/* 反例：处理函数里 malloc。主程序正在 malloc 内部持有 arena 锁时信号到达，
 * 处理函数再取同一把锁，进程停在那里。
 *
 *   ./unsafe       处理函数里 malloc
 *   ./unsafe flag  处理函数里只设标志
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_ticks = 0;

static void on_tick_malloc(int signo) {
    (void)signo;
    void *p = malloc(4096);
    free(p);
}

static void on_tick_flag(int signo) {
    (void)signo;
    g_ticks++;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int use_flag = argc > 1 && strcmp(argv[1], "flag") == 0;

    struct sigaction sa = {0};
    sa.sa_handler = use_flag ? on_tick_flag : on_tick_malloc;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        die("sigaction");
    }

    timer_t timer;
    struct sigevent event = {0};
    event.sigev_notify = SIGEV_SIGNAL;
    event.sigev_signo = SIGALRM;
    if (timer_create(CLOCK_MONOTONIC, &event, &timer) == -1) {
        die("timer_create");
    }

    /* 周期取最短，让信号尽量落在 malloc 内部持有 arena 锁的那一小段。 */
    struct itimerspec spec = {0};
    spec.it_value.tv_nsec = 1;
    spec.it_interval.tv_nsec = 1;
    if (timer_settime(timer, 0, &spec, NULL) == -1) {
        die("timer_settime");
    }

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        void *p = malloc(4096);
        free(p);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long elapsed_ms = (long long)(now.tv_sec - start.tv_sec) * 1000LL +
                               (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed_ms > 1000) {
            break;
        }
    }

    timer_delete(timer);
    printf("main loop finished, ticks=%d\n", (int)g_ticks);
    return 0;
}