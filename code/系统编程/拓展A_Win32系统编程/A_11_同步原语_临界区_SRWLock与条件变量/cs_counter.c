/* 用临界区对象修复 race.c 的数据竞争。 */
#include <stdio.h>
#include <windows.h>

#define ITERS 1000000

static CRITICAL_SECTION lock;
static int counter = 0;

static DWORD WINAPI worker(LPVOID arg) {
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        EnterCriticalSection(&lock);
        counter++;
        LeaveCriticalSection(&lock);
    }
    return 0;
}

int main(void) {
    HANDLE threads[2];

    InitializeCriticalSection(&lock);
    for (int i = 0; i < 2; i++) {
        threads[i] = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (threads[i] == NULL) {
            printf("CreateThread failed: %lu\n", GetLastError());
            return 1;
        }
    }
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    for (int i = 0; i < 2; i++) {
        CloseHandle(threads[i]);
    }
    DeleteCriticalSection(&lock);

    printf("counter = %d, expected %d\n", counter, 2 * ITERS);
    return 0;
}