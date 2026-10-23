/* 完成端口作工作队列时，并发值限制的是「可运行」的线程数，不是线程总数。
 * 6 个任务各占 100 ms，3 个工作线程；并发值 1 时，忙等的任务只能一个接一个跑，
 * 而 Sleep 的线程不算可运行，三个线程都能拿到任务。 */
#include <stdio.h>
#include <windows.h>

#define WORKERS 3
#define TASKS 6
#define WORK_MS 100

/* 忙等 ms 毫秒，期间线程一直占着 CPU。 */
static void spin(int ms) {
    ULONGLONG end = GetTickCount64() + (ULONGLONG)ms;
    volatile unsigned long x = 0;

    while (GetTickCount64() < end) {
        for (int i = 0; i < 10000; i++) {
            x += (unsigned long)i;
        }
    }
}

static DWORD WINAPI worker(LPVOID arg) {
    HANDLE port = (HANDLE)arg;
    DWORD count;
    ULONG_PTR key;
    OVERLAPPED *ov;

    for (;;) {
        if (!GetQueuedCompletionStatus(port, &count, &key, &ov, INFINITE)) {
            return 1;
        }
        if (key == 0) {
            return 0;
        }
        printf("  任务 %llu：线程 %lu\n", (unsigned long long)key,
               GetCurrentThreadId());
        if (count == 0) {
            spin(WORK_MS);
        } else {
            Sleep(WORK_MS);
        }
    }
}

static void run(DWORD concurrency, DWORD mode) {
    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0,
                                         concurrency);
    HANDLE threads[WORKERS];
    ULONGLONG start;

    for (int i = 0; i < WORKERS; i++) {
        threads[i] = CreateThread(NULL, 0, worker, port, 0, NULL);
    }
    printf("并发值 %lu，任务%s %d ms：\n", (unsigned long)concurrency,
           mode == 0 ? "忙等" : "Sleep", WORK_MS);
    start = GetTickCount64();
    for (int i = 1; i <= TASKS; i++) {
        PostQueuedCompletionStatus(port, mode, (ULONG_PTR)i, NULL);
    }
    Sleep(20);
    for (int i = 0; i < WORKERS; i++) {
        PostQueuedCompletionStatus(port, 2, 0, NULL);
    }
    WaitForMultipleObjects(WORKERS, threads, TRUE, INFINITE);
    printf("  总计 %llu ms\n", (unsigned long long)(GetTickCount64() - start));
    for (int i = 0; i < WORKERS; i++) {
        CloseHandle(threads[i]);
    }
    CloseHandle(port);
}

int main(void) {
    run(1, 0);
    run(1, 1);
    return 0;
}