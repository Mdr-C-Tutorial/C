#define _POSIX_C_SOURCE 200809L

/* 对照实验：先检查标志再 pause 会丢唤醒，sigsuspend 不会。
 *
 *   ./lost_wakeup             缺陷版：信号落在检查之后、pause 之前
 *   ./lost_wakeup sigsuspend  正确版
 *
 * 程序自己 fork 一个子进程，150 ms 后给父进程发 SIGUSR1，
 * 模拟「信号恰好在检查与睡眠之间到达」。
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_wake = 0;

static void on_usr1(int signo) {
    (void)signo;
    g_wake = 1;
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

int main(int argc, char *argv[]) {
    int use_sigsuspend = argc > 1 && strcmp(argv[1], "sigsuspend") == 0;

    struct sigaction sa = {0};
    sa.sa_handler = on_usr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        die("sigaction");
    }

    pid_t child = fork();
    if (child == -1) {
        die("fork");
    }
    if (child == 0) {
        sleep_ms(150);
        kill(getppid(), SIGUSR1);
        _exit(0);
    }

    if (use_sigsuspend) {
        sigset_t block, old;
        sigemptyset(&block);
        sigaddset(&block, SIGUSR1);
        if (sigprocmask(SIG_BLOCK, &block, &old) == -1) {
            die("sigprocmask");
        }
        printf("ready\n");
        fflush(stdout);

        while (!g_wake) {
            sigsuspend(&old);
        }
        printf("woke up, flag=%d\n", (int)g_wake);
    } else {
        printf("ready\n");
        fflush(stdout);
        sleep_ms(300);
        pause();
        printf("woke up, flag=%d\n", (int)g_wake);
    }

    waitpid(child, NULL, 0);
    return 0;
}