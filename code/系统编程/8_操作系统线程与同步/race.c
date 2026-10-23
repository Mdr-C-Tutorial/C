#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define THREADS 2
#define ITERS 100000

static long counter;

static void *worker(void *arg) {
    (void)arg;

    for (int i = 0; i < ITERS; i++) {
        counter++;
    }
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];

    for (int i = 0; i < THREADS; i++) {
        if (pthread_create(&t[i], NULL, worker, NULL) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(t[i], NULL);
    }
    printf("counter = %ld (expected %d)\n", counter, THREADS * ITERS);
    return 0;
}