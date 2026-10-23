#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_wake = 0;

static void on_alarm(int signo) {
    (void)signo;
    g_wake = 1;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        die("sigaction");
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        die("pipe");
    }

    alarm(1);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    struct pollfd pfd = {pipefd[0], POLLIN, 0};
    int ready = poll(&pfd, 1, 5000);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000LL +
                           (now.tv_nsec - start.tv_nsec) / 1000000;

    if (ready == -1) {
        printf("poll: %s after %lld ms, flag=%d\n", strerror(errno),
               elapsed_ms, (int)g_wake);
    } else {
        printf("poll returned %d (revents=0x%x) after %lld ms, flag=%d\n",
               ready, pfd.revents, elapsed_ms, (int)g_wake);
    }
    return 0;
}