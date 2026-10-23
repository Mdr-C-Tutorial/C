#define _POSIX_C_SOURCE 200809L

/* 无竞争地等待 SIGUSR1 或 SIGTERM：先阻塞，再在 sigsuspend 里原子地放开。 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t g_usr1 = 0;
static volatile sig_atomic_t g_stop = 0;

static void on_usr1(int signo) {
    (void)signo;
    g_usr1 = 1;
}

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
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_usr1;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        die("sigaction SIGUSR1");
    }
    sa.sa_handler = on_stop;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        die("sigaction SIGTERM");
    }

    sigset_t block, old;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    sigaddset(&block, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &block, &old) == -1) {
        die("sigprocmask");
    }

    printf("pid %ld, waiting for SIGUSR1 or SIGTERM\n", (long)getpid());
    fflush(stdout);

    while (!g_stop) {
        while (!g_stop && !g_usr1) {
            sigsuspend(&old);
        }
        if (g_usr1) {
            g_usr1 = 0;
            printf("got SIGUSR1\n");
            fflush(stdout);
        }
    }
    printf("got SIGTERM, bye\n");
    fflush(stdout);
    return 0;
}