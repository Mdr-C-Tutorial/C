#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WORKERS 2
#define TASKS 20000

/*
 * 缺陷版线程池：两个工作线程取任务并累加。
 *
 * 已知现象：结果偶尔偏小，程序打印完结果后经常退不出来。
 * 用 -fsanitize=thread 编译运行可以看到数据竞争报告。
 */

struct pool {
    pthread_mutex_t lock;
    pthread_cond_t has_work;
    long pending;   /* 队列里还有多少任务 */
    long done;      /* 已完成的任务数 */
    long sum;       /* 已完成任务的数值之和 */
    int closing;    /* 不再有新任务 */
};

static struct pool pool = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .has_work = PTHREAD_COND_INITIALIZER,
    .pending = 0,
    .done = 0,
    .sum = 0,
    .closing = 0,
};

static void *worker(void *arg) {
    (void)arg;

    for (;;) {
        pthread_mutex_lock(&pool.lock);
        while (pool.pending == 0 && !pool.closing) {
            pthread_cond_wait(&pool.has_work, &pool.lock);
        }
        if (pool.pending == 0) {
            pthread_mutex_unlock(&pool.lock);
            break;
        }
        pool.pending--;
        pthread_mutex_unlock(&pool.lock);

        long value = 1;
        pool.done++;
        pool.sum += value;
    }
    return NULL;
}

int main(void) {
    pthread_t t[WORKERS];

    for (int i = 0; i < WORKERS; i++) {
        if (pthread_create(&t[i], NULL, worker, NULL) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    for (int i = 0; i < TASKS; i++) {
        pthread_mutex_lock(&pool.lock);
        pool.pending++;
        pthread_cond_signal(&pool.has_work);
        pthread_mutex_unlock(&pool.lock);
    }

    /* 等线程把手上的活干完，再宣告结束。 */
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 100 * 1000000L};

    nanosleep(&ts, NULL);

    pthread_mutex_lock(&pool.lock);
    pool.closing = 1;
    pthread_cond_signal(&pool.has_work);
    pthread_mutex_unlock(&pool.lock);

    printf("done = %ld, sum = %ld (expected %d)\n", pool.done, pool.sum, TASKS);
    fflush(stdout);

    for (int i = 0; i < WORKERS; i++) {
        pthread_join(t[i], NULL);
    }
    printf("joined all workers\n");
    return 0;
}