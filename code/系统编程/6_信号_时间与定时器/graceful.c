#define _POSIX_C_SOURCE 200809L

/* 收到 SIGINT 或 SIGTERM 后把标志置 1，主循环看到标志再收尾退出。 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;

static void on_stop(int signo) {
    (void)signo;
    g_stop = 1;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        die("sigaction SIGINT");
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        die("sigaction SIGTERM");
    }

    printf("pid %ld, running\n", (long)getpid());
    fflush(stdout);

    while (!g_stop) {
        struct timespec step = {0, 200 * 1000 * 1000};
        if (nanosleep(&step, NULL) == -1 && errno != EINTR) {
            die("nanosleep");
        }
    }

    printf("caught stop signal, cleaning up\n");
    return 0;
}