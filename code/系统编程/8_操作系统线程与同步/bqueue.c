#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CAP 4
#define PRODUCERS 3
#define CONSUMERS 2
#define ITEMS 12

struct bqueue {
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int buf[CAP];
    size_t head;
    size_t len;
    int closed;
};

struct consumer_stat {
    int id;
    long count;
    long sum;
};

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void bq_init(struct bqueue *q) {
    pthread_condattr_t attr;

    die_rc(pthread_mutex_init(&q->lock, NULL), "pthread_mutex_init");
    /* not_empty 用单调时钟，bq_pop_timed 的截止时刻才不会被校时打乱。 */
    die_rc(pthread_condattr_init(&attr), "pthread_condattr_init");
    die_rc(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC),
           "pthread_condattr_setclock");
    die_rc(pthread_cond_init(&q->not_empty, &attr), "pthread_cond_init");
    pthread_condattr_destroy(&attr);
    die_rc(pthread_cond_init(&q->not_full, NULL), "pthread_cond_init");
    q->head = 0;
    q->len = 0;
    q->closed = 0;
}

static void bq_destroy(struct bqueue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

/* 队列满时阻塞，直到有人取走一项或队列被关闭。 */
static int bq_push(struct bqueue *q, int value) {
    die_rc(pthread_mutex_lock(&q->lock), "pthread_mutex_lock");
    while (q->len == CAP && !q->closed) {
        die_rc(pthread_cond_wait(&q->not_full, &q->lock), "pthread_cond_wait");
    }
    if (q->closed) {
        die_rc(pthread_mutex_unlock(&q->lock), "pthread_mutex_unlock");
        return -1;
    }
    q->buf[(q->head + q->len) % CAP] = value;
    q->len++;
    die_rc(pthread_cond_signal(&q->not_empty), "pthread_cond_signal");
    die_rc(pthread_mutex_unlock(&q->lock), "pthread_mutex_unlock");
    return 0;
}

/* 队列空时阻塞；返回 0 表示取到一项，-1 表示队列已关闭且空了。 */
static int bq_pop(struct bqueue *q, int *out) {
    die_rc(pthread_mutex_lock(&q->lock), "pthread_mutex_lock");
    while (q->len == 0 && !q->closed) {
        die_rc(pthread_cond_wait(&q->not_empty, &q->lock), "pthread_cond_wait");
    }
    if (q->len == 0) {
        die_rc(pthread_mutex_unlock(&q->lock), "pthread_mutex_unlock");
        return -1;
    }
    *out = q->buf[q->head];
    q->head = (q->head + 1) % CAP;
    q->len--;
    die_rc(pthread_cond_signal(&q->not_full), "pthread_cond_signal");
    die_rc(pthread_mutex_unlock(&q->lock), "pthread_mutex_unlock");
    return 0;
}

/* 最多等 ms 毫秒。返回 0 表示取到一项，-1 表示超时或队列已关闭。 */
static int bq_pop_timed(struct bqueue *q, int *out, long ms) {
    struct timespec abs;

    clock_gettime(CLOCK_MONOTONIC, &abs);
    abs.tv_sec += ms / 1000;
    abs.tv_nsec += (ms % 1000) * 1000000L;
    if (abs.tv_nsec >= 1000000000L) {
        abs.tv_sec += 1;
        abs.tv_nsec -= 1000000000L;
    }
    die_rc(pthread_mutex_lock(&q->lock), "pthread_mutex_lock");
    while (q->len == 0 && !q->closed) {
        int rc = pthread_cond_timedwait(&q->not_empty, &q->lock, &abs);

        if (rc == ETIMEDOUT) {
            die_rc(pthread_mutex_unlock(&q->lock), "pthread_mutex_unlock");
            return -1;
        }
        die_rc(rc, "pthread_cond_timedwait");
    }
    if (q->len == 0) {
        die_rc(pthread_mutex_unlock(&q->lock), "pthread_mutex_unlock");
        return -1;
    }
    *out = q->buf[q->head];
    q->head = (q->head + 1) % CAP;
    q->len--;
    die_rc(pthread_cond_signal(&q->not_full), "pthread_cond_signal");
    die_rc(pthread_mutex_unlock(&q->lock), "pthread_mutex_unlock");
    return 0;
}

static void bq_close(struct bqueue *q) {
    die_rc(pthread_mutex_lock(&q->lock), "pthread_mutex_lock");
    q->closed = 1;
    die_rc(pthread_cond_broadcast(&q->not_empty), "pthread_cond_broadcast");
    die_rc(pthread_cond_broadcast(&q->not_full), "pthread_cond_broadcast");
    die_rc(pthread_mutex_unlock(&q->lock), "pthread_mutex_unlock");
}

struct producer_arg {
    struct bqueue *q;
    int base;
};

struct consumer_arg {
    struct bqueue *q;
    struct consumer_stat *stat;
};

static void *producer(void *arg) {
    struct producer_arg *p = arg;

    for (int i = 0; i < ITEMS; i++) {
        if (bq_push(p->q, p->base + i) != 0) {
            break;
        }
    }
    return NULL;
}

static void *consumer(void *arg) {
    struct consumer_arg *c = arg;
    int value;

    while (bq_pop(c->q, &value) == 0) {
        c->stat->count++;
        c->stat->sum += value;
    }
    return NULL;
}

int main(void) {
    struct bqueue q;
    pthread_t prod[PRODUCERS];
    pthread_t cons[CONSUMERS];
    struct producer_arg pargs[PRODUCERS];
    struct consumer_arg cargs[CONSUMERS];
    struct consumer_stat stats[CONSUMERS] = {{0, 0, 0}, {1, 0, 0}};
    long total = 0;

    bq_init(&q);
    int probe;

    if (bq_pop_timed(&q, &probe, 100) == 0) {
        printf("timed pop got %d from an empty queue\n", probe);
    } else {
        printf("empty queue: timed pop returned -1 after 100 ms\n");
    }
    for (int i = 0; i < CONSUMERS; i++) {
        cargs[i].q = &q;
        cargs[i].stat = &stats[i];
        die_rc(pthread_create(&cons[i], NULL, consumer, &cargs[i]), "pthread_create");
    }
    for (int i = 0; i < PRODUCERS; i++) {
        pargs[i].q = &q;
        pargs[i].base = i * 1000;
        die_rc(pthread_create(&prod[i], NULL, producer, &pargs[i]), "pthread_create");
    }
    for (int i = 0; i < PRODUCERS; i++) {
        die_rc(pthread_join(prod[i], NULL), "pthread_join");
        for (int k = 0; k < ITEMS; k++) {
            total += pargs[i].base + k;
        }
    }
    bq_close(&q);
    for (int i = 0; i < CONSUMERS; i++) {
        die_rc(pthread_join(cons[i], NULL), "pthread_join");
    }
    for (int i = 0; i < CONSUMERS; i++) {
        printf("consumer %d: %2ld items, sum = %ld\n", stats[i].id, stats[i].count,
               stats[i].sum);
    }
    printf("produced %d items, sum = %ld\n", PRODUCERS * ITEMS, total);
    bq_destroy(&q);
    return 0;
}