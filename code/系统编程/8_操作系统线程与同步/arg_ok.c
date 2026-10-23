#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREADS 4

struct job {
    int id;
};

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void *show(void *arg) {
    struct job *job = arg;

    printf("thread sees id = %d\n", job->id);
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];
    struct job jobs[THREADS];

    for (int i = 0; i < THREADS; i++) {
        jobs[i].id = i;
        die_rc(pthread_create(&t[i], NULL, show, &jobs[i]), "pthread_create");
    }
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_join(t[i], NULL), "pthread_join");
    }
    return 0;
}