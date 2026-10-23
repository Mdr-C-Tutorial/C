#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define THREADS 4
#define SLOTS 2

static sem_t slots;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static int setup_done;

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void shared_setup(void) {
    setup_done = 1;
    printf("setup ran once\n");
}

static void *worker(void *arg) {
    int id = (int)(long)arg;
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 150 * 1000000L};

    die_rc(pthread_once(&once, shared_setup), "pthread_once");
    if (sem_wait(&slots) != 0) {
        perror("sem_wait");
        return NULL;
    }
    printf("thread %d: enter (setup_done = %d)\n", id, setup_done);
    fflush(stdout);
    nanosleep(&ts, NULL);
    printf("thread %d: leave\n", id);
    fflush(stdout);
    sem_post(&slots);
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];

    if (sem_init(&slots, 0, SLOTS) != 0) {
        perror("sem_init");
        return 1;
    }
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_create(&t[i], NULL, worker, (void *)(long)i), "pthread_create");
    }
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_join(t[i], NULL), "pthread_join");
    }
    int value;

    sem_getvalue(&slots, &value);
    printf("slots left = %d\n", value);
    sem_destroy(&slots);
    return 0;
}