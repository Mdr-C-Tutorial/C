#define _GNU_SOURCE

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define THREADS 3

static atomic_int pending;

static void on_signal(int sig) {
    (void)sig;
    atomic_store(&pending, 1);
}

static void sleep_ms(long ms) {
    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};

    while (nanosleep(&ts, &ts) == -1) {
    }
}

static void *worker(void *arg) {
    (void)arg;
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    /* 新建线程继承了主线程的掩码，这里解除阻塞，让它能收到进程信号。 */
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    for (;;) {
        sleep_ms(2);
        if (atomic_exchange(&pending, 0) != 0) {
            printf("  SIGUSR1 handled in tid %ld\n", (long)gettid());
            fflush(stdout);
        }
    }
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];
    struct sigaction sa = {0};
    sigset_t set;

    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    /* 先阻塞再创建线程：主线程永远不接收这个信号。 */
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    for (int i = 0; i < THREADS; i++) {
        if (pthread_create(&t[i], NULL, worker, NULL) != 0) {
            return 1;
        }
    }
    printf("main tid = %ld, sending SIGUSR1 three times\n",
           (long)gettid());
    fflush(stdout);
    for (int i = 0; i < 3; i++) {
        sleep_ms(80);
        kill(getpid(), SIGUSR1);
    }
    sleep_ms(100);
    _exit(0);
}