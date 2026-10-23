#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define THREADS 4

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void *show(void *arg) {
    int *id = arg;
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 10 * 1000 * 1000};

    nanosleep(&ts, NULL);
    printf("thread sees id = %d\n", *id);
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];

    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_create(&t[i], NULL, show, &i), "pthread_create");
    }
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_join(t[i], NULL), "pthread_join");
    }
    return 0;
}