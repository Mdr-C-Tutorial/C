#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREADS 4
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

static void *worker(void *arg) {
    (void)arg;

    for (int i = 0; i < ITERS; i++) {
        die_rc(pthread_mutex_lock(&lock), "pthread_mutex_lock");
        counter++;
        die_rc(pthread_mutex_unlock(&lock), "pthread_mutex_unlock");
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int nthreads = 1;
    pthread_t t[THREADS];

    if (argc > 1) {
        nthreads = atoi(argv[1]);
        if (nthreads < 1 || nthreads > THREADS) {
            fprintf(stderr, "usage: %s [1..%d]\n", argv[0], THREADS);
            return 1;
        }
    }
    for (int i = 0; i < nthreads; i++) {
        die_rc(pthread_create(&t[i], NULL, worker, NULL), "pthread_create");
    }
    for (int i = 0; i < nthreads; i++) {
        die_rc(pthread_join(t[i], NULL), "pthread_join");
    }
    printf("counter = %ld\n", counter);
    return 0;
}