#define _POSIX_C_SOURCE 200809L

/* 标准信号不排队：阻塞 SIGUSR1 期间连发三次，解除阻塞后只递送一次。 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t g_count = 0;

static void on_usr1(int signo) {
    (void)signo;
    g_count++;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_usr1;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        die("sigaction SIGUSR1");
    }

    sigset_t block, old, pending;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &block, &old) == -1) {
        die("sigprocmask block");
    }

    printf("SIGUSR1 blocked, sending it three times\n");
    fflush(stdout);
    raise(SIGUSR1);
    raise(SIGUSR1);
    raise(SIGUSR1);

    if (sigpending(&pending) == -1) {
        die("sigpending");
    }
    printf("pending before unblock: %d\n", sigismember(&pending, SIGUSR1));

    if (sigprocmask(SIG_SETMASK, &old, NULL) == -1) {
        die("sigprocmask restore");
    }
    printf("delivered after unblock: %d\n", (int)g_count);
    return 0;
}