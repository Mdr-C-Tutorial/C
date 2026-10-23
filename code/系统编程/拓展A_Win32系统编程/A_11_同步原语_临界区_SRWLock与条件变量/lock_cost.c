/* 无争用时三种锁的加锁/解锁开销：临界区对象、SRW 锁、互斥量对象。 */
#include <stdio.h>
#include <windows.h>

#define ITERS 1000000

int main(void) {
    LARGE_INTEGER freq;
    LARGE_INTEGER t0;
    LARGE_INTEGER t1;
    CRITICAL_SECTION cs;
    SRWLOCK srw = SRWLOCK_INIT;
    HANDLE mutex;
    volatile int sink = 0;

    QueryPerformanceFrequency(&freq);
    mutex = CreateMutexW(NULL, FALSE, NULL);
    if (mutex == NULL) {
        printf("CreateMutexW failed: %lu\n", GetLastError());
        return 1;
    }
    InitializeCriticalSection(&cs);

    QueryPerformanceCounter(&t0);
    for (int i = 0; i < ITERS; i++) {
        EnterCriticalSection(&cs);
        sink++;
        LeaveCriticalSection(&cs);
    }
    QueryPerformanceCounter(&t1);
    printf("critical section: %8.1f ms\n",
           (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart);

    QueryPerformanceCounter(&t0);
    for (int i = 0; i < ITERS; i++) {
        AcquireSRWLockExclusive(&srw);
        sink++;
        ReleaseSRWLockExclusive(&srw);
    }
    QueryPerformanceCounter(&t1);
    printf("SRW lock:         %8.1f ms\n",
           (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart);

    QueryPerformanceCounter(&t0);
    for (int i = 0; i < ITERS; i++) {
        WaitForSingleObject(mutex, INFINITE);
        sink++;
        ReleaseMutex(mutex);
    }
    QueryPerformanceCounter(&t1);
    printf("mutex object:     %8.1f ms\n",
           (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart);

    DeleteCriticalSection(&cs);
    CloseHandle(mutex);
    return 0;
}