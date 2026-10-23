#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STACK_SIZE (256 * 1024)
#define THREADS 2

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void *worker(void *arg) {
    const char *name = arg;
    pthread_attr_t attr;
    size_t size;
    void *addr;

    die_rc(pthread_getattr_np(pthread_self(), &attr), "pthread_getattr_np");
    die_rc(pthread_attr_getstack(&attr, &addr, &size), "pthread_attr_getstack");
    printf("%-6s stack: %zu KiB at %p\n", name, size / 1024, addr);
    pthread_attr_destroy(&attr);
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];
    pthread_attr_t attr;

    die_rc(pthread_attr_init(&attr), "pthread_attr_init");
    die_rc(pthread_attr_setstacksize(&attr, STACK_SIZE), "pthread_attr_setstacksize");
    die_rc(pthread_create(&t[0], &attr, worker, "small"), "pthread_create");
    die_rc(pthread_create(&t[1], NULL, worker, "default"), "pthread_create");
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_join(t[i], NULL), "pthread_join");
    }
    pthread_attr_destroy(&attr);
    return 0;
}