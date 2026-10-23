#define _POSIX_C_SOURCE 200809L

/* 实时信号会排队，并且随信号带一个整数值。 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t g_values[8];
static volatile sig_atomic_t g_count = 0;

static void on_rt(int signo, siginfo_t *info, void *context) {
    (void)signo;
    (void)context;
    if (g_count < 8) {
        g_values[g_count] = (sig_atomic_t)info->si_value.sival_int;
    }
    g_count++;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_sigaction = on_rt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        die("sigaction SIGRTMIN");
    }

    sigset_t block, old;
    sigemptyset(&block);
    sigaddset(&block, SIGRTMIN);
    if (sigprocmask(SIG_BLOCK, &block, &old) == -1) {
        die("sigprocmask");
    }

    for (int i = 1; i <= 3; i++) {
        union sigval value;
        value.sival_int = i * 100;
        if (sigqueue(getpid(), SIGRTMIN, value) == -1) {
            die("sigqueue");
        }
    }

    if (sigprocmask(SIG_SETMASK, &old, NULL) == -1) {
        die("sigprocmask restore");
    }

    printf("received %d signals:", (int)g_count);
    for (int i = 0; i < (int)g_count && i < 8; i++) {
        printf(" %d", (int)g_values[i]);
    }
    printf("\n");
    return 0;
}