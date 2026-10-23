#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREADS 4

struct job {
    int id;
    long n;
};

struct result {
    int id;
    long sum;
};

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void *work(void *arg) {
    struct job *job = arg;
    struct result *res = malloc(sizeof *res);

    if (res == NULL) {
        return NULL;
    }
    long sum = 0;

    for (long i = 1; i <= job->n; i++) {
        sum += i;
    }
    res->id = job->id;
    res->sum = sum;
    printf("thread %d done\n", job->id);
    return res; /* 返回值由 join 的一方负责释放 */
}

int main(void) {
    pthread_t t[THREADS];
    struct job jobs[THREADS];
    long total = 0;

    for (int i = 0; i < THREADS; i++) {
        jobs[i].id = i;
        jobs[i].n = (long)(i + 1) * 100000;
        die_rc(pthread_create(&t[i], NULL, work, &jobs[i]), "pthread_create");
    }
    for (int i = 0; i < THREADS; i++) {
        struct result *res;

        die_rc(pthread_join(t[i], (void **)&res), "pthread_join");
        printf("join: thread %d returned sum = %ld\n", res->id, res->sum);
        total += res->sum;
        free(res);
    }
    printf("total = %ld\n", total);
    return 0;
}