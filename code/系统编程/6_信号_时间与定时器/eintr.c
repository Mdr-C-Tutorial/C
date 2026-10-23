#define _POSIX_C_SOURCE 200809L

/* 演示 EINTR 与 SA_RESTART：alarm 一秒后发 SIGALRM，read 正阻塞在管道上。
 *
 *   ./eintr            read 不自动重启
 *   ./eintr restart    read 自动重启
 *   ./eintr - sleep    nanosleep 两种设置下都会返回 EINTR
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_alarmed = 0;

static void on_alarm(int signo) {
    (void)signo;
    g_alarmed = 1;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(int argc, char *argv[]) {
    int restart = argc > 1 && strcmp(argv[1], "restart") == 0;
    int use_sleep = argc > 2 && strcmp(argv[2], "sleep") == 0;

    struct sigaction sa = {0};
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = restart ? SA_RESTART : 0;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        die("sigaction SIGALRM");
    }

    alarm(1);
    double start = now_ms();

    if (use_sleep) {
        struct timespec req = {3, 0};
        struct timespec left = {0, 0};
        int rc = nanosleep(&req, &left);
        double elapsed = now_ms() - start;
        printf("nanosleep returned %d (%s), %ld.%09ld s left, after %.1f ms\n",
               rc, rc == -1 ? strerror(errno) : "completed", (long)left.tv_sec,
               left.tv_nsec, elapsed);
        return 0;
    }

    char buf[64];
    ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
    double elapsed = now_ms() - start;

    if (n == -1) {
        printf("read: %s (errno=%d) after %.1f ms\n", strerror(errno), errno,
               elapsed);
    } else {
        printf("read returned %zd (%s) after %.1f ms\n", n,
               n == 0 ? "end of file" : "data", elapsed);
    }
    return 0;
}