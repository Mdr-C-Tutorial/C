/* SRW 锁的两种模式：4 个读线程共享读，1 个写线程每隔一会儿更新配置。 */
#include <stdio.h>
#include <windows.h>

#define READERS 4
#define ROUNDS 120

struct config {
    SRWLOCK lock;
    int version;
    int timeout_ms;
};

static struct config g_cfg = {.lock = SRWLOCK_INIT, .timeout_ms = 100};
static volatile LONG g_active_readers = 0;
static volatile LONG g_max_readers = 0;
static volatile LONG g_inconsistent = 0;
static volatile LONG g_version_hits[4];

static DWORD WINAPI reader(LPVOID arg) {
    (void)arg;

    for (int r = 0; r < ROUNDS; r++) {
        LONG active;

        AcquireSRWLockShared(&g_cfg.lock);
        active = InterlockedIncrement(&g_active_readers);
        if (active > g_max_readers) {
            InterlockedExchange(&g_max_readers, active);
        }
        /* 在锁内把要用的字段复制出来，锁外慢慢用 */
        int version = g_cfg.version;
        int timeout = g_cfg.timeout_ms;
        if (timeout != 100 * (version + 1)) {
            InterlockedIncrement(&g_inconsistent);
        }
        InterlockedIncrement(&g_version_hits[version]);
        Sleep(2);
        InterlockedDecrement(&g_active_readers);
        ReleaseSRWLockShared(&g_cfg.lock);
        Sleep(1);
    }
    return 0;
}

static DWORD WINAPI writer(LPVOID arg) {
    (void)arg;
    for (int v = 1; v <= 3; v++) {
        Sleep(100);
        AcquireSRWLockExclusive(&g_cfg.lock);
        Sleep(5); /* 模拟重新装载配置 */
        g_cfg.version = v;
        g_cfg.timeout_ms = 100 * (v + 1);
        printf("writer:  version=%d timeout=%d\n", v, g_cfg.timeout_ms);
        ReleaseSRWLockExclusive(&g_cfg.lock);
    }
    return 0;
}

int main(void) {
    HANDLE readers[READERS];
    HANDLE w;

    for (int i = 0; i < READERS; i++) {
        readers[i] = CreateThread(NULL, 0, reader, (LPVOID)(INT_PTR)i, 0, NULL);
        if (readers[i] == NULL) {
            printf("CreateThread failed: %lu\n", GetLastError());
            return 1;
        }
    }
    w = CreateThread(NULL, 0, writer, NULL, 0, NULL);
    if (w == NULL) {
        printf("CreateThread failed: %lu\n", GetLastError());
        return 1;
    }

    WaitForMultipleObjects(READERS, readers, TRUE, INFINITE);
    WaitForSingleObject(w, INFINITE);
    for (int i = 0; i < READERS; i++) {
        CloseHandle(readers[i]);
    }
    CloseHandle(w);
    printf("reads by version: v0=%ld v1=%ld v2=%ld v3=%ld\n", g_version_hits[0],
           g_version_hits[1], g_version_hits[2], g_version_hits[3]);
    printf("max concurrent readers = %ld, inconsistent reads = %ld\n", g_max_readers,
           g_inconsistent);
    return 0;
}