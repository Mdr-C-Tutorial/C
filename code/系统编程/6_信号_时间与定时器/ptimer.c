#define _POSIX_C_SOURCE 200809L

/* POSIX 定时器：100 ms 周期，先挡住 SIGALRM 350 ms，观察信号合并与 overrun。 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_ticks = 0;

static void on_tick(int signo) {
    (void)signo;
    g_ticks++;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void sleep_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
    }
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_tick;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        die("sigaction");
    }

    sigset_t block, old;
    sigemptyset(&block);
    sigaddset(&block, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &block, &old) == -1) {
        die("sigprocmask block");
    }

    timer_t timer;
    struct sigevent event = {0};
    event.sigev_notify = SIGEV_SIGNAL;
    event.sigev_signo = SIGALRM;
    if (timer_create(CLOCK_MONOTONIC, &event, &timer) == -1) {
        die("timer_create");
    }

    struct itimerspec spec = {0};
    spec.it_value.tv_nsec = 100 * 1000000L;
    spec.it_interval.tv_nsec = 100 * 1000000L;
    if (timer_settime(timer, 0, &spec, NULL) == -1) {
        die("timer_settime");
    }

    printf("timer running at 100 ms, SIGALRM blocked for 350 ms\n");
    sleep_ms(350);

    if (sigprocmask(SIG_SETMASK, &old, NULL) == -1) {
        die("sigprocmask restore");
    }
    sleep_ms(20);

    printf("handler ran %d time(s)\n", (int)g_ticks);
    printf("timer_getoverrun after unblock: %d\n", timer_getoverrun(timer));

    sleep_ms(300);
    printf("after another 300 ms: handler ran %d time(s)\n", (int)g_ticks);
    timer_delete(timer);
    return 0;
}