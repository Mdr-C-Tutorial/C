#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITERS 100000

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static long counter;

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

/* 只有一个线程：每次加锁都走快路径，strace 里看不到任何 futex 调用。 */
int main(void) {
    for (int i = 0; i < ITERS; i++) {
        die_rc(pthread_mutex_lock(&lock), "pthread_mutex_lock");
        counter++;
        die_rc(pthread_mutex_unlock(&lock), "pthread_mutex_unlock");
    }
    printf("counter = %ld\n", counter);
    return 0;
}
