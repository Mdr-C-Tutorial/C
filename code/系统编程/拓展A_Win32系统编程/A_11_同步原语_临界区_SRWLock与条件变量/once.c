/* 一次性初始化：多个线程同时要配置，回调只执行一次。 */
#include <stdio.h>
#include <windows.h>

struct config {
    int port;
    int timeout_ms;
};

static INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
static struct config g_cfg;
static volatile LONG g_runs = 0;

static BOOL CALLBACK init_config(PINIT_ONCE once, PVOID param, PVOID *context) {
    (void)once;
    (void)param;
    InterlockedIncrement(&g_runs);
    printf("init_config runs on thread %lu\n", GetCurrentThreadId());
    Sleep(50); /* 模拟读配置文件这类慢操作 */
    g_cfg.port = 23456;
    g_cfg.timeout_ms = 500;
    *context = &g_cfg;
    return TRUE;
}

static DWORD WINAPI worker(LPVOID arg) {
    int id = (int)(INT_PTR)arg;
    PVOID context = NULL;

    if (!InitOnceExecuteOnce(&g_once, init_config, NULL, &context)) {
        printf("worker %d: InitOnceExecuteOnce failed: %lu\n", id, GetLastError());
        return 1;
    }
    struct config *cfg = context;
    printf("worker %d: port=%d timeout=%d\n", id, cfg->port, cfg->timeout_ms);
    return 0;
}

int main(void) {
    HANDLE threads[4];

    for (int i = 0; i < 4; i++) {
        threads[i] = CreateThread(NULL, 0, worker, (LPVOID)(INT_PTR)i, 0, NULL);
        if (threads[i] == NULL) {
            printf("CreateThread failed: %lu\n", GetLastError());
            return 1;
        }
    }
    WaitForMultipleObjects(4, threads, TRUE, INFINITE);
    for (int i = 0; i < 4; i++) {
        CloseHandle(threads[i]);
    }
    printf("callback ran %ld time(s)\n", g_runs);
    return 0;
}