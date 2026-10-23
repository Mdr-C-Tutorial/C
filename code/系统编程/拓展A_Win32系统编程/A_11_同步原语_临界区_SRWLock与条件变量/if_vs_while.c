/* 条件变量等待写成 if 与写成 while 的差别：两个消费者都在等，生产者只放一个。 */
#include <stdio.h>
#include <windows.h>

struct queue {
    SRWLOCK lock;
    CONDITION_VARIABLE cv;
    int arrived;
    int count;
    int item;
    int closed;
};

static void wait_until_both_waiting(struct queue *q) {
    for (;;) {
        int arrived;
        AcquireSRWLockExclusive(&q->lock);
        arrived = q->arrived;
        ReleaseSRWLockExclusive(&q->lock);
        if (arrived == 2) {
            return;
        }
        Sleep(1);
    }
}

struct ctx {
    struct queue *q;
    int id;
};

static DWORD WINAPI consumer_if_body(LPVOID arg) {
    struct ctx *c = arg;
    struct queue *q = c->q;

    AcquireSRWLockExclusive(&q->lock);
    q->arrived++;
    if (q->count == 0) {
        SleepConditionVariableSRW(&q->cv, &q->lock, INFINITE, 0);
    }
    /* 错误：醒来后没有重新检查条件 */
    int value = q->item;
    q->count--;
    printf("if   consumer %d: value=%d, count after take = %d\n", c->id, value, q->count);
    ReleaseSRWLockExclusive(&q->lock);
    return 0;
}

static DWORD WINAPI consumer_while_body(LPVOID arg) {
    struct ctx *c = arg;
    struct queue *q = c->q;

    AcquireSRWLockExclusive(&q->lock);
    q->arrived++;
    while (q->count == 0 && !q->closed) {
        SleepConditionVariableSRW(&q->cv, &q->lock, INFINITE, 0);
    }
    if (q->count == 0) {
        printf("while consumer %d: closed, no item\n", c->id);
        ReleaseSRWLockExclusive(&q->lock);
        return 0;
    }
    int value = q->item;
    q->count--;
    printf("while consumer %d: value=%d, count after take = %d\n", c->id, value,
           q->count);
    ReleaseSRWLockExclusive(&q->lock);
    return 0;
}

int main(void) {
    HANDLE threads[2];
    struct ctx ctxs[2];
    struct queue q = {.lock = SRWLOCK_INIT, .cv = CONDITION_VARIABLE_INIT};

    /* 错误版本：if 等待 */
    for (int i = 0; i < 2; i++) {
        ctxs[i].q = &q;
        ctxs[i].id = i;
        threads[i] = CreateThread(NULL, 0, consumer_if_body, &ctxs[i], 0, NULL);
        if (threads[i] == NULL) {
            printf("CreateThread failed: %lu\n", GetLastError());
            return 1;
        }
    }
    wait_until_both_waiting(&q);
    AcquireSRWLockExclusive(&q.lock);
    q.item = 42;
    q.count = 1;
    ReleaseSRWLockExclusive(&q.lock);
    WakeAllConditionVariable(&q.cv);
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    for (int i = 0; i < 2; i++) {
        CloseHandle(threads[i]);
    }

    printf("---\n");

    /* 正确版本：while 等待 */
    q.arrived = 0;
    q.count = 0;
    q.item = 0;
    q.closed = 0;
    for (int i = 0; i < 2; i++) {
        ctxs[i].q = &q;
        ctxs[i].id = i;
        threads[i] = CreateThread(NULL, 0, consumer_while_body, &ctxs[i], 0, NULL);
        if (threads[i] == NULL) {
            printf("CreateThread failed: %lu\n", GetLastError());
            return 1;
        }
    }
    wait_until_both_waiting(&q);
    AcquireSRWLockExclusive(&q.lock);
    q.item = 42;
    q.count = 1;
    ReleaseSRWLockExclusive(&q.lock);
    WakeAllConditionVariable(&q.cv);
    Sleep(100);
    AcquireSRWLockExclusive(&q.lock);
    q.closed = 1;
    ReleaseSRWLockExclusive(&q.lock);
    WakeAllConditionVariable(&q.cv);
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    for (int i = 0; i < 2; i++) {
        CloseHandle(threads[i]);
    }
    return 0;
}