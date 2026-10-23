#define _POSIX_C_SOURCE 200809L

/* SA_SIGINFO 的处理函数从 siginfo_t 里取出发送者的 PID 和 UID。 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_got = 0;
static volatile sig_atomic_t g_signo = 0;
static volatile sig_atomic_t g_from_pid = 0;
static volatile sig_atomic_t g_from_uid = 0;

static void on_usr1(int signo, siginfo_t *info, void *context) {
    (void)context;
    g_signo = signo;
    g_from_pid = (sig_atomic_t)info->si_pid;
    g_from_uid = (sig_atomic_t)info->si_uid;
    g_got = 1;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_sigaction = on_usr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        die("sigaction SIGUSR1");
    }

    pid_t child = fork();
    if (child == -1) {
        die("fork");
    }
    if (child == 0) {
        struct timespec delay = {0, 200 * 1000 * 1000};
        nanosleep(&delay, NULL);
        kill(getppid(), SIGUSR1);
        _exit(0);
    }

    while (!g_got) {
        struct timespec step = {0, 10 * 1000 * 1000};
        nanosleep(&step, NULL);
    }

    printf("signal %d from pid %ld, uid %ld\n", (int)g_signo,
           (long)g_from_pid, (long)g_from_uid);
    printf("child pid is %ld\n", (long)child);

    waitpid(child, NULL, 0);
    return 0;
}