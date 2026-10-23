#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void *signal_thread(void *arg) {
    sigset_t *set = arg;
    int sig;

    for (;;) {
        int rc = sigwait(set, &sig);
        if (rc != 0) {
            die_rc(rc, "sigwait");
        }
        printf("handled %s in thread %lu\n", strsignal(sig),
               (unsigned long)pthread_self());
        fflush(stdout);
        if (sig == SIGTERM) {
            printf("shutting down\n");
            fflush(stdout);
            break;
        }
    }
    return NULL;
}

int main(void) {
    sigset_t set;
    pthread_t t;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    /* 先阻塞，再创建处理线程：没有线程会漏掉这两个信号。 */
    die_rc(pthread_sigmask(SIG_BLOCK, &set, NULL), "pthread_sigmask");
    die_rc(pthread_create(&t, NULL, signal_thread, &set), "pthread_create");

    printf("main: pid = %d, SIGINT and SIGTERM blocked in all threads\n",
           (int)getpid());
    fflush(stdout);
    die_rc(pthread_join(t, NULL), "pthread_join");
    printf("main: joined\n");
    return 0;
}