#define _POSIX_C_SOURCE 200809L

#include "tpool.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TASKS 40

static pthread_mutex_t out_lock = PTHREAD_MUTEX_INITIALIZER;
static int finished;

struct job {
    int id;
};

static void sleep_ms(long ms) {
    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};

    while (nanosleep(&ts, &ts) == -1) {
    }
}

static void job_run(void *arg) {
    struct job *job = arg;

    sleep_ms(5);
    pthread_mutex_lock(&out_lock);
    finished++;
    printf("task %02d done on thread %lu\n", job->id, (unsigned long)pthread_self());
    fflush(stdout);
    pthread_mutex_unlock(&out_lock);
    free(job);
}

int main(void) {
    tpool *pool = tpool_create(3);

    if (pool == NULL) {
        perror("tpool_create");
        return 1;
    }
    for (int i = 0; i < TASKS; i++) {
        struct job *job = malloc(sizeof *job);

        if (job == NULL) {
            break;
        }
        job->id = i;
        if (tpool_submit(pool, job_run, job) != 0) {
            perror("tpool_submit");
            free(job);
            break;
        }
    }
    printf("submitted %d tasks, shutting down\n", TASKS);
    tpool_shutdown(pool);
    printf("finished %d tasks\n", finished);
    printf("submitting after shutdown: %s\n",
           tpool_submit(pool, job_run, NULL) == 0 ? "accepted" : "rejected");
    tpool_destroy(pool);
    return 0;
}