#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREADS 4
#define ROUNDS 3

static pthread_barrier_t barrier;
static long partial[THREADS];
static long total;
static long round_sum[ROUNDS];

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void busy(long n) {
    volatile long sink = 0;

    for (long i = 0; i < n; i++) {
        sink += i;
    }
}

static int barrier_wait(void) {
    int rc = pthread_barrier_wait(&barrier);

    if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
        die_rc(rc, "pthread_barrier_wait");
    }
    return rc;
}

static void *worker(void *arg) {
    int id = (int)(long)arg;

    for (int r = 0; r < ROUNDS; r++) {
        /* 阶段一：各自算自己那一份，各线程耗时不同。 */
        partial[id] = (long)(id + 1) * 1000;
        busy((long)(id + 1) * 3000000);
        barrier_wait();

        /* 阶段二：汇总，此时所有 partial 都写完了。 */
        if (id == 0) {
            total = 0;
            for (int k = 0; k < THREADS; k++) {
                total += partial[k];
            }
            round_sum[r] = total;
        }
        barrier_wait();

        /* 阶段三：清空，为下一轮准备。 */
        partial[id] = 0;
        barrier_wait();
    }
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];

    die_rc(pthread_barrier_init(&barrier, NULL, THREADS), "pthread_barrier_init");
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_create(&t[i], NULL, worker, (void *)(long)i), "pthread_create");
    }
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_join(t[i], NULL), "pthread_join");
    }
    for (int r = 0; r < ROUNDS; r++) {
        printf("round %d: total = %ld\n", r, round_sum[r]);
    }
    die_rc(pthread_barrier_destroy(&barrier), "pthread_barrier_destroy");
    return 0;
}