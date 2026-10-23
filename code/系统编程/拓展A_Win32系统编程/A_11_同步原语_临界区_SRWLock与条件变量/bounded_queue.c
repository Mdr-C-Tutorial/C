/* 容量为 4 的有界队列：2 个生产者、2 个消费者，用条件变量等待。 */
#include <stdio.h>
#include <windows.h>

#define CAPACITY 4
#define PER_PRODUCER 8
#define PRODUCERS 2
#define CONSUMERS 2

struct queue {
    SRWLOCK lock;
    CONDITION_VARIABLE not_empty;
    CONDITION_VARIABLE not_full;
    int slot[CAPACITY];
    int head;
    int tail;
    int count;
    int closed;
    int produced;
    int consumed;
    int sum;
    int per_consumer[CONSUMERS];
};

static struct queue g_q = {
    .lock = SRWLOCK_INIT,
    .not_empty = CONDITION_VARIABLE_INIT,
    .not_full = CONDITION_VARIABLE_INIT,
};

static void put(struct queue *q, int value) {
    AcquireSRWLockExclusive(&q->lock);
    while (q->count == CAPACITY) {
        SleepConditionVariableSRW(&q->not_full, &q->lock, INFINITE, 0);
    }
    q->slot[q->tail] = value;
    q->tail = (q->tail + 1) % CAPACITY;
    q->count++;
    q->produced++;
    ReleaseSRWLockExclusive(&q->lock);
    /* 先改状态再唤醒；唤醒放在锁外，少一次上下文切换 */
    WakeConditionVariable(&q->not_empty);
}

/* 返回取到的值；队列已关闭且为空时返回 -1。 */
static int get(struct queue *q, int id) {
    int value;

    AcquireSRWLockExclusive(&q->lock);
    while (q->count == 0 && !q->closed) {
        SleepConditionVariableSRW(&q->not_empty, &q->lock, INFINITE, 0);
    }
    if (q->count == 0) {
        ReleaseSRWLockExclusive(&q->lock);
        return -1;
    }
    value = q->slot[q->head];
    q->head = (q->head + 1) % CAPACITY;
    q->count--;
    q->consumed++;
    q->sum += value;
    q->per_consumer[id]++;
    ReleaseSRWLockExclusive(&q->lock);
    WakeConditionVariable(&q->not_full);
    return value;
}

static DWORD WINAPI producer(LPVOID arg) {
    int id = (int)(INT_PTR)arg;

    for (int i = 0; i < PER_PRODUCER; i++) {
        put(&g_q, id * 100 + i);
    }
    return 0;
}

static DWORD WINAPI consumer(LPVOID arg) {
    int id = (int)(INT_PTR)arg;

    while (get(&g_q, id) >= 0) {
    }
    return 0;
}

int main(void) {
    HANDLE producers[PRODUCERS];
    HANDLE consumers[CONSUMERS];

    for (int i = 0; i < CONSUMERS; i++) {
        consumers[i] = CreateThread(NULL, 0, consumer, (LPVOID)(INT_PTR)i, 0, NULL);
        if (consumers[i] == NULL) {
            printf("CreateThread failed: %lu\n", GetLastError());
            return 1;
        }
    }
    for (int i = 0; i < PRODUCERS; i++) {
        producers[i] = CreateThread(NULL, 0, producer, (LPVOID)(INT_PTR)i, 0, NULL);
        if (producers[i] == NULL) {
            printf("CreateThread failed: %lu\n", GetLastError());
            return 1;
        }
    }

    WaitForMultipleObjects(PRODUCERS, producers, TRUE, INFINITE);
    AcquireSRWLockExclusive(&g_q.lock);
    g_q.closed = 1;
    ReleaseSRWLockExclusive(&g_q.lock);
    WakeAllConditionVariable(&g_q.not_empty);

    if (WaitForMultipleObjects(CONSUMERS, consumers, TRUE, INFINITE) == WAIT_FAILED) {
        printf("WaitForMultipleObjects failed: %lu\n", GetLastError());
        return 1;
    }

    printf("produced %d, consumed %d, sum %d, left in queue %d\n", g_q.produced,
           g_q.consumed, g_q.sum, g_q.count);
    printf("consumer 0 took %d, consumer 1 took %d\n", g_q.per_consumer[0],
           g_q.per_consumer[1]);

    for (int i = 0; i < PRODUCERS; i++) {
        CloseHandle(producers[i]);
    }
    for (int i = 0; i < CONSUMERS; i++) {
        CloseHandle(consumers[i]);
    }
    return 0;
}