#define _POSIX_C_SOURCE 200809L

#include "tpool.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

struct task {
    tpool_fn fn;
    void *arg;
    struct task *next;
};

struct tpool {
    pthread_mutex_t lock;
    pthread_cond_t has_work;
    pthread_cond_t idle;
    pthread_t *threads;
    int nthreads;
    struct task *head;
    struct task *tail;
    int stopping;
    int active;
    int shutdown;
};

static void *worker(void *arg) {
    tpool *pool = arg;

    for (;;) {
        pthread_mutex_lock(&pool->lock);
        while (pool->head == NULL && !pool->stopping) {
            pthread_cond_wait(&pool->has_work, &pool->lock);
        }
        if (pool->head == NULL) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }
        struct task *task = pool->head;

        pool->head = task->next;
        if (pool->head == NULL) {
            pool->tail = NULL;
        }
        pool->active++;
        pthread_mutex_unlock(&pool->lock);

        task->fn(task->arg);
        free(task);

        pthread_mutex_lock(&pool->lock);
        pool->active--;
        if (pool->active == 0 && pool->head == NULL) {
            pthread_cond_broadcast(&pool->idle);
        }
        pthread_mutex_unlock(&pool->lock);
    }
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
    pool->nthreads = nthreads;
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
    if (pthread_cond_init(&pool->has_work, NULL) != 0 ||
        pthread_cond_init(&pool->idle, NULL) != 0) {
        pthread_mutex_destroy(&pool->lock);
        free(pool->threads);
        free(pool);
        return NULL;
    }
    for (int i = 0; i < nthreads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker, pool) != 0) {
            pool->stopping = 1;
            pthread_cond_broadcast(&pool->has_work);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            pthread_mutex_destroy(&pool->lock);
            pthread_cond_destroy(&pool->has_work);
            pthread_cond_destroy(&pool->idle);
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
    struct task *task = malloc(sizeof *task);

    if (task == NULL) {
        return -1;
    }
    task->fn = fn;
    task->arg = arg;
    task->next = NULL;

    pthread_mutex_lock(&pool->lock);
    if (pool->stopping) {
        pthread_mutex_unlock(&pool->lock);
        free(task);
        errno = ESHUTDOWN;
        return -1;
    }
    if (pool->tail != NULL) {
        pool->tail->next = task;
    } else {
        pool->head = task;
    }
    pool->tail = task;
    pthread_cond_signal(&pool->has_work);
    pthread_mutex_unlock(&pool->lock);
    return 0;
}

int tpool_shutdown(tpool *pool) {
    if (pool->shutdown) {
        errno = EINVAL;
        return -1;
    }
    pool->shutdown = 1;
    pthread_mutex_lock(&pool->lock);
    pool->stopping = 1;
    pthread_cond_broadcast(&pool->has_work);
    pthread_mutex_unlock(&pool->lock);
    for (int i = 0; i < pool->nthreads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    return 0;
}

void tpool_destroy(tpool *pool) {
    if (pool == NULL) {
        return;
    }
    if (!pool->shutdown) {
        tpool_shutdown(pool);
    }
    struct task *task = pool->head;

    while (task != NULL) {
        struct task *next = task->next;

        free(task);
        task = next;
    }
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->has_work);
    pthread_cond_destroy(&pool->idle);
    free(pool->threads);
    free(pool);
}