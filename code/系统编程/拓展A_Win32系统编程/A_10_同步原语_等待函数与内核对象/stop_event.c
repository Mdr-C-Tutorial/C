#include <windows.h>
#include <stdio.h>

#define N_WORKERS 3

static HANDLE g_stop;
static HANDLE g_work;

struct worker_arg {
    int id;
    int served;
};

static DWORD WINAPI worker(void *param) {
    struct worker_arg *arg = param;
    HANDLE waits[2];

    waits[0] = g_stop; /* 停止事件放在下标 0，两个都有信号时它优先 */
    waits[1] = g_work;
    for (;;) {
        DWORD r = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

        if (r == WAIT_OBJECT_0) {
            break;
        }
        if (r == WAIT_OBJECT_0 + 1) {
            arg->served++;
            printf("worker %d: item %d\n", arg->id, arg->served);
        }
    }
    printf("worker %d: stop, served %d item(s)\n", arg->id, arg->served);
    return 0;
}

int main(void) {
    struct worker_arg args[N_WORKERS];
    HANDLE threads[N_WORKERS];
    int posted = 6;
    int total = 0;
    int i;

    /* 先看清「下标小的优先」：两个对象同时有信号，返回下标 0 的那一个，
       下标 1 的信号量计数不会被消耗 */
    {
        HANDLE probe[2];
        DWORD r;
        LONG prev = -1;

        probe[0] = CreateEventW(NULL, TRUE, FALSE, NULL);
        probe[1] = CreateSemaphoreW(NULL, 0, 10, NULL);
        SetEvent(probe[0]);
        ReleaseSemaphore(probe[1], 1, NULL);
        r = WaitForMultipleObjects(2, probe, FALSE, 0);
        printf("both signaled: returned index %lu\n", (unsigned long)r);
        if (WaitForSingleObject(probe[1], 0) == WAIT_OBJECT_0) {
            ReleaseSemaphore(probe[1], 1, &prev);
            printf("the semaphore still had its count\n");
        }
        CloseHandle(probe[0]);
        CloseHandle(probe[1]);
    }

    g_stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_work = CreateSemaphoreW(NULL, 0, 100, NULL);
    if (g_stop == NULL || g_work == NULL) {
        printf("create failed: %lu\n", GetLastError());
        return 1;
    }

    for (i = 0; i < N_WORKERS; i++) {
        args[i].id = i;
        args[i].served = 0;
        threads[i] = CreateThread(NULL, 0, worker, &args[i], 0, NULL);
    }

    for (i = 0; i < posted; i++) {
        ReleaseSemaphore(g_work, 1, NULL);
        Sleep(20);
    }
    Sleep(100);
    printf("main: posted %d item(s), request stop\n", posted);

    SetEvent(g_stop);
    WaitForMultipleObjects(N_WORKERS, threads, TRUE, 2000);
    for (i = 0; i < N_WORKERS; i++) {
        total += args[i].served;
        CloseHandle(threads[i]);
    }
    printf("main: %d of %d item(s) served\n", total, posted);

    CloseHandle(g_stop);
    CloseHandle(g_work);
    return 0;
}