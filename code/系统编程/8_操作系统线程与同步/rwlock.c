#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READERS 4
#define WRITES 200000

/* 读写都要求 x + y 恒等于 100。 */
struct config {
    pthread_rwlock_t rw;
    int x;
    int y;
    int use_lock;
};

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void read_pair(struct config *c, int *x, int *y) {
    if (c->use_lock) {
        die_rc(pthread_rwlock_rdlock(&c->rw), "pthread_rwlock_rdlock");
    }
    *x = c->x;
    *y = c->y;
    if (c->use_lock) {
        die_rc(pthread_rwlock_unlock(&c->rw), "pthread_rwlock_unlock");
    }
}

static void *reader(void *arg) {
    struct config *c = arg;
    long broken = 0;

    for (int i = 0; i < WRITES; i++) {
        int x;
        int y;

        read_pair(c, &x, &y);
        if (x + y != 100) {
            broken++;
        }
    }
    long *out = malloc(sizeof *out);

    if (out == NULL) {
        return NULL;
    }
    *out = broken;
    return out;
}

static void *writer(void *arg) {
    struct config *c = arg;

    for (int i = 0; i < WRITES; i++) {
        die_rc(pthread_rwlock_wrlock(&c->rw), "pthread_rwlock_wrlock");
        c->x++;
        c->y--;
        die_rc(pthread_rwlock_unlock(&c->rw), "pthread_rwlock_unlock");
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    struct config c = {.x = 50, .y = 50, .use_lock = 1};
    pthread_t r[READERS];
    pthread_t w;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--nolock") == 0) {
            c.use_lock = 0;
        } else {
            fprintf(stderr, "usage: %s [--nolock]\n", argv[0]);
            return 1;
        }
    }
    die_rc(pthread_rwlock_init(&c.rw, NULL), "pthread_rwlock_init");
    for (int i = 0; i < READERS; i++) {
        die_rc(pthread_create(&r[i], NULL, reader, &c), "pthread_create");
    }
    die_rc(pthread_create(&w, NULL, writer, &c), "pthread_create");
    die_rc(pthread_join(w, NULL), "pthread_join");
    long total = 0;

    for (int i = 0; i < READERS; i++) {
        long *broken;

        die_rc(pthread_join(r[i], (void **)&broken), "pthread_join");
        total += *broken;
        free(broken);
    }
    printf("%s: reads = %d, inconsistent snapshots = %ld\n",
           c.use_lock ? "read lock" : "no lock", READERS * WRITES, total);
    printf("final x = %d, y = %d\n", c.x, c.y);
    die_rc(pthread_rwlock_destroy(&c.rw), "pthread_rwlock_destroy");
    return 0;
}