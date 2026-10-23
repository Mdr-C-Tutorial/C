#define _POSIX_C_SOURCE 200809L

/* SIGCHLD 合并实验：五个子进程同时退出，处理函数只跑一次。
 * 每次只 waitpid 一次留四个僵尸，循环 waitpid 才能全部回收。
 *
 *   ./reap once   处理函数里只 waitpid 一次
 *   ./reap loop   处理函数里循环到 WNOHANG 返回 0
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHILDREN 5

static volatile sig_atomic_t g_reaped = 0;
static volatile sig_atomic_t g_handlers = 0;
static int g_loop_mode = 0;

static void on_chld(int signo) {
    (void)signo;
    int saved = errno;
    g_handlers++;
    if (g_loop_mode) {
        while (waitpid(-1, NULL, WNOHANG) > 0) {
            g_reaped++;
        }
    } else if (waitpid(-1, NULL, WNOHANG) > 0) {
        g_reaped++;
    }
    errno = saved;
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
    g_loop_mode = argc > 1 && argv[1][0] == 'l';

    struct sigaction sa = {0};
    sa.sa_handler = on_chld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        die("sigaction SIGCHLD");
    }

    /* 先把 SIGCHLD 挡住，让五个子进程的退出挤成一次递送。 */
    sigset_t block, old;
    sigemptyset(&block);
    sigaddset(&block, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &block, &old) == -1) {
        die("sigprocmask");
    }

    for (int i = 0; i < CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            die("fork");
        }
        if (pid == 0) {
            sleep_ms(50);
            _exit(0);
        }
    }

    sleep_ms(300);
    if (sigprocmask(SIG_SETMASK, &old, NULL) == -1) {
        die("sigprocmask restore");
    }
    sleep_ms(200);

    printf("parent pid %ld, %d children exited, handler ran %d time(s), "
           "reaped %d\n",
           (long)getpid(), CHILDREN, (int)g_handlers, (int)g_reaped);
    fflush(stdout);

    /* 留三秒，方便在另一个终端看还挂在进程表里的僵尸。 */
    sleep_ms(3000);
    return 0;
}