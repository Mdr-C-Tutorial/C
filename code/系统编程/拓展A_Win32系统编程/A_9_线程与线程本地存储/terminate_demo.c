/* terminate_demo.c：一个线程拿到临界区之后一直不释放，主线程用 TerminateThread
 * 结束它，再试着进入同一个临界区。 */
#include <process.h>
#include <stdio.h>
#include <windows.h>

static CRITICAL_SECTION g_cs;
static volatile LONG g_ticks;

static unsigned __stdcall worker(void *param) {
    (void)param;
    EnterCriticalSection(&g_cs);
    printf("worker: holding the critical section\n");
    for (;;) {
        InterlockedIncrement(&g_ticks);
        Sleep(10);
    }
    return 0;
}

int main(void) {
    HANDLE thread;
    DWORD exit_code = 0;
    LONG ticks_before;

    InitializeCriticalSection(&g_cs);
    thread = (HANDLE)_beginthreadex(NULL, 0, worker, NULL, 0, NULL);
    if (thread == NULL) {
        printf("_beginthreadex failed: %lu\n", GetLastError());
        DeleteCriticalSection(&g_cs);
        return 1;
    }
    Sleep(100);
    ticks_before = g_ticks;

    TerminateThread(thread, 1234);
    WaitForSingleObject(thread, INFINITE);
    GetExitCodeThread(thread, &exit_code);
    printf("after TerminateThread: exit code %lu\n", (unsigned long)exit_code);

    Sleep(100);
    printf("worker iterations: %ld before, %ld after\n", (long)ticks_before,
           (long)g_ticks);
    printf("TryEnterCriticalSection: %d (0 means still owned)\n",
           TryEnterCriticalSection(&g_cs));

    CloseHandle(thread);
    /* 临界区对象永远拿不回来，这里不能 DeleteCriticalSection */
    return 0;
}