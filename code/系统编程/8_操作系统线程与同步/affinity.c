#define _GNU_SOURCE

#include <pthread.h>
#include <sched.h>
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

static void *worker(void *arg) {
    (void)arg;
    cpu_set_t set;

    CPU_ZERO(&set);
    die_rc(pthread_getaffinity_np(pthread_self(), sizeof set, &set),
           "pthread_getaffinity_np");
    printf("thread may run on %d CPU(s), currently CPU %d\n", CPU_COUNT(&set),
           sched_getcpu());
    return NULL;
}

int main(void) {
    pthread_t t;
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);

    printf("sysconf(_SC_NPROCESSORS_ONLN) = %ld\n", ncpu);
    die_rc(pthread_create(&t, NULL, worker, NULL), "pthread_create");
    die_rc(pthread_join(t, NULL), "pthread_join");
    return 0;
}