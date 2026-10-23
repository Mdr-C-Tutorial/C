#include <windows.h>
#include <stdio.h>

static HANDLE g_mutex;
static HANDLE g_go;
static int g_shared; /* 受互斥量保护 */

static DWORD WINAPI owner(void *param) {
    int release_first = *(int *)param;

    WaitForSingleObject(g_mutex, INFINITE);
    printf("owner: acquired the mutex, writes g_shared\n");
    g_shared = 42;
    if (release_first) {
        ReleaseMutex(g_mutex);
        printf("owner: released before exiting\n");
    }
    return 0; /* 未释放就结束，互斥量被遗弃 */
}

static DWORD WINAPI second(void *param) {
    DWORD r;

    (void)param;
    WaitForSingleObject(g_go, INFINITE);
    r = WaitForSingleObject(g_mutex, INFINITE);
    if (r == WAIT_ABANDONED) {
        printf("second: WAIT_ABANDONED, g_shared = %d\n", g_shared);
    } else {
        printf("second: WAIT_OBJECT_0, g_shared = %d\n", g_shared);
    }
    ReleaseMutex(g_mutex);
    return 0;
}

static DWORD WINAPI probe(void *param) {
    DWORD r;

    (void)param;
    r = WaitForSingleObject(g_mutex, 300);
    printf("  probe thread waits 300 ms: %s\n",
           r == WAIT_OBJECT_0 ? "acquired" : "still owned");
    if (r == WAIT_OBJECT_0) {
        ReleaseMutex(g_mutex);
    }
    return 0;
}

int main(void) {
    HANDLE threads[2];
    int release_first;

    for (release_first = 0; release_first <= 1; release_first++) {
        g_mutex = CreateMutexW(NULL, FALSE, NULL);
        g_go = CreateEventW(NULL, TRUE, FALSE, NULL);
        g_shared = 0;
        printf("owner %s:\n", release_first ? "releases normally"
                                            : "exits without releasing");
        threads[0] = CreateThread(NULL, 0, owner, &release_first, 0, NULL);
        threads[1] = CreateThread(NULL, 0, second, NULL, 0, NULL);
        WaitForSingleObject(threads[0], INFINITE);
        Sleep(0);
        SetEvent(g_go);
        WaitForSingleObject(threads[1], INFINITE);
        CloseHandle(threads[0]);
        CloseHandle(threads[1]);
        CloseHandle(g_mutex);
        CloseHandle(g_go);
    }

    /* 同一线程重复获取，就要释放同样的次数 */
    printf("recursive acquisition from the main thread:\n");
    g_mutex = CreateMutexW(NULL, TRUE, NULL); /* 创建时即拥有 */
    WaitForSingleObject(g_mutex, INFINITE);

    threads[0] = CreateThread(NULL, 0, probe, NULL, 0, NULL);
    WaitForSingleObject(threads[0], INFINITE);

    printf("  ReleaseMutex #1: %d\n", ReleaseMutex(g_mutex));
    threads[0] = CreateThread(NULL, 0, probe, NULL, 0, NULL);
    WaitForSingleObject(threads[0], INFINITE);

    printf("  ReleaseMutex #2: %d\n", ReleaseMutex(g_mutex));
    threads[0] = CreateThread(NULL, 0, probe, NULL, 0, NULL);
    WaitForSingleObject(threads[0], INFINITE);
    CloseHandle(threads[0]);

    if (!ReleaseMutex(g_mutex)) {
        printf("  one more ReleaseMutex fails, error %lu\n", GetLastError());
    }
    CloseHandle(g_mutex);
    return 0;
}