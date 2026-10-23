#define _POSIX_C_SOURCE 200809L

#include "tpool.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#define QUEUE_CAP 64

struct task {
    tpool_fn fn;
    void *arg;
};

struct tpool {
    pthread_mutex_t lock;
    pthread_cond_t has_task; /* 队列非空，工作线程可以取任务 */
    pthread_cond_t has_slot; /* 队列不满，提交者可以放任务 */
    struct task queue[QUEUE_CAP];
    size_t head;
    size_t len;
    int running;  /* 工作线程是否继续取任务 */
    int shut;     /* shutdown 是否已经做过（保证只 join 一次） */
    int nthreads; /* 工作线程总数 */
    pthread_t *threads;
};

static void check(int rc) {
    /* 锁和条件变量自身的失败正常运行时不会发生。 */
    if (rc != 0) {
        abort();
    }
}

static void *worker(void *arg) {
    tpool *pool = arg;

    check(pthread_mutex_lock(&pool->lock));
    for (;;) {
        while (pool->len == 0 && pool->running) {
            check(pthread_cond_wait(&pool->has_task, &pool->lock));
        }
        if (pool->len == 0) {
            break; /* running 已经为 0，队列也空了 */
        }
        struct task task = pool->queue[pool->head];

        pool->head = (pool->head + 1) % QUEUE_CAP;
        pool->len--;
        check(pthread_cond_signal(&pool->has_slot));
        check(pthread_mutex_unlock(&pool->lock));

        task.fn(task.arg);
        check(pthread_mutex_lock(&pool->lock));
    }
    check(pthread_mutex_unlock(&pool->lock));
    return NULL;
}

tpool *tpool_create(int nthreads) {
    if (nthreads <= 0) {
        errno = EINVAL;
        return NULL;
    }
    tpool *pool = calloc(1, sizeof *pool);

    if (pool == NULL) {
        return NULL;
    }
    pool->threads = calloc((size_t)nthreads, sizeof *pool->threads);
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool->threads);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->has_task, NULL) != 0) {
        pthread_mutex_destroy(&pool->lock);
        free(pool->threads);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->has_slot, NULL) != 0) {
        pthread_cond_destroy(&pool->has_task);
        pthread_mutex_destroy(&pool->lock);
        free(pool->threads);
        free(pool);
        return NULL;
    }
    pool->nthreads = nthreads;
    pool->running = 1;
    for (int i = 0; i < nthreads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker, pool) != 0) {
            /* 已创建的工作线程看到 running 变成 0 后退出。 */
            check(pthread_mutex_lock(&pool->lock));
            pool->running = 0;
            check(pthread_cond_broadcast(&pool->has_task));
            check(pthread_mutex_unlock(&pool->lock));
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            pthread_cond_destroy(&pool->has_slot);
            pthread_cond_destroy(&pool->has_task);
            pthread_mutex_destroy(&pool->lock);
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }
    return pool;
}

int tpool_submit(tpool *pool, tpool_fn fn, void *arg) {
    if (fn == NULL) {
        errno = EINVAL;
        return -1;
    }
    check(pthread_mutex_lock(&pool->lock));
    if (!pool->running) {
        check(pthread_mutex_unlock(&pool->lock));
        errno = ESHUTDOWN;
        return -1;
    }
    while (pool->len == QUEUE_CAP && pool->running) {
        /* 队列满就等，等到有工作线程取走一项；提交者在这里被挡住就是背压。 */
        check(pthread_cond_wait(&pool->has_slot, &pool->lock));
    }
    if (!pool->running) {
        check(pthread_mutex_unlock(&pool->lock));
        errno = ESHUTDOWN;
        return -1;
    }
    pool->queue[(pool->head + pool->len) % QUEUE_CAP] =
        (struct task){.fn = fn, .arg = arg};
    pool->len++;
    check(pthread_cond_signal(&pool->has_task));
    check(pthread_mutex_unlock(&pool->lock));
    return 0;
}

int tpool_shutdown(tpool *pool) {
    if (pool == NULL) {
        errno = EINVAL;
        return -1;
    }
    check(pthread_mutex_lock(&pool->lock));
    if (!pool->shut) {
        pool->shut = 1;
        pool->running = 0;
        check(pthread_cond_broadcast(&pool->has_task));
        check(pthread_cond_broadcast(&pool->has_slot));
    }
    check(pthread_mutex_unlock(&pool->lock));
    for (int i = 0; i < pool->nthreads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    pool->nthreads = 0;
    return 0;
}

void tpool_destroy(tpool *pool) {
    if (pool == NULL) {
        return;
    }
    tpool_shutdown(pool);
    pthread_cond_destroy(&pool->has_slot);
    pthread_cond_destroy(&pool->has_task);
    pthread_mutex_destroy(&pool->lock);
    free(pool->threads);
    free(pool);
}