/* 数据竞争：两个线程各把同一个普通 int 加 1000000 次。 */
#include <stdio.h>
#include <windows.h>

#define ITERS 1000000

static int counter = 0;
static volatile LONG g_start = 0;

static DWORD WINAPI worker(LPVOID arg) {
    (void)arg;
    /* 等主线程放行，保证两个线程真正同时开跑 */
    while (InterlockedCompareExchange(&g_start, 0, 0) == 0) {
        Sleep(0);
    }
    for (int i = 0; i < ITERS; i++) {
        counter++;
    }
    return 0;
}

int main(void) {
    HANDLE threads[2];

    for (int i = 0; i < 2; i++) {
        threads[i] = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (threads[i] == NULL) {
            printf("CreateThread failed: %lu\n", GetLastError());
            return 1;
        }
    }
    InterlockedExchange(&g_start, 1);
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    for (int i = 0; i < 2; i++) {
        CloseHandle(threads[i]);
    }

    printf("counter = %d, expected %d\n", counter, 2 * ITERS);
    return 0;
}