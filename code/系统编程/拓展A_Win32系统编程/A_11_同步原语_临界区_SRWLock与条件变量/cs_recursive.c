/* 临界区对象可以重复进入：同一线程进几次就要出几次；别的线程进不来。 */
#include <stdio.h>
#include <windows.h>

static CRITICAL_SECTION g_lock;
static volatile LONG g_stage = 0;
static volatile LONG g_try_result = -1;

static DWORD WINAPI prober(LPVOID arg) {
    (void)arg;
    while (InterlockedCompareExchange(&g_stage, 0, 0) == 0) {
        Sleep(1); /* 等主线程把锁拿到手 */
    }
    InterlockedExchange(&g_try_result, TryEnterCriticalSection(&g_lock));
    if (g_try_result) {
        LeaveCriticalSection(&g_lock);
    }
    return 0;
}

int main(void) {
    HANDLE h;

    InitializeCriticalSection(&g_lock);
    h = CreateThread(NULL, 0, prober, NULL, 0, NULL);
    if (h == NULL) {
        printf("CreateThread failed: %lu\n", GetLastError());
        return 1;
    }

    /* 同一线程连续进入两次，两次都立即成功 */
    EnterCriticalSection(&g_lock);
    EnterCriticalSection(&g_lock);
    printf("entered twice without blocking\n");

    /* 只离开一次，所有权仍然属于本线程 */
    LeaveCriticalSection(&g_lock);
    InterlockedExchange(&g_stage, 1);
    WaitForSingleObject(h, INFINITE);
    printf("other thread's TryEnter after one leave -> %ld\n", g_try_result);

    /* 再离开一次，锁才真正释放 */
    LeaveCriticalSection(&g_lock);
    CloseHandle(h);
    h = CreateThread(NULL, 0, prober, NULL, 0, NULL);
    if (h == NULL) {
        printf("CreateThread failed: %lu\n", GetLastError());
        return 1;
    }
    InterlockedExchange(&g_stage, 1);
    WaitForSingleObject(h, INFINITE);
    printf("other thread's TryEnter after two leaves -> %ld\n", g_try_result);

    CloseHandle(h);
    DeleteCriticalSection(&g_lock);
    return 0;
}