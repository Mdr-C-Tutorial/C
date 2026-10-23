#include <windows.h>
#include <stdio.h>

#define WORKERS 3

static HANDLE g_start;
static LARGE_INTEGER g_freq;
static int g_started[WORKERS];
static double g_elapsed[WORKERS];

struct worker_arg {
    int id;
};

static double ms_since(LARGE_INTEGER t0) {
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - t0.QuadPart) * 1000.0 /
           (double)g_freq.QuadPart;
}

static DWORD WINAPI worker(void *param) {
    struct worker_arg *arg = param;
    LARGE_INTEGER begin;
    DWORD r;

    QueryPerformanceCounter(&begin);
    r = WaitForSingleObject(g_start, 500);
    g_elapsed[arg->id] = ms_since(begin);
    g_started[arg->id] = (r == WAIT_OBJECT_0);
    return 0;
}

static void run_round(const char *label, BOOL manual_reset) {
    HANDLE threads[WORKERS];
    struct worker_arg args[WORKERS];
    int i;

    printf("%s\n", label);
    g_start = CreateEventW(NULL, manual_reset, FALSE, NULL);
    if (g_start == NULL) {
        printf("CreateEventW failed: %lu\n", GetLastError());
        return;
    }
    for (i = 0; i < WORKERS; i++) {
        args[i].id = i;
        g_started[i] = 0;
        g_elapsed[i] = 0.0;
        threads[i] = CreateThread(NULL, 0, worker, &args[i], 0, NULL);
    }

    Sleep(100); /* 让工作线程先进入等待 */
    SetEvent(g_start);

    WaitForMultipleObjects(WORKERS, threads, TRUE, INFINITE);
    for (i = 0; i < WORKERS; i++) {
        printf("  worker %d: %-9s after %5.1f ms\n", i,
               g_started[i] ? "started" : "timed out", g_elapsed[i]);
        CloseHandle(threads[i]);
    }
    CloseHandle(g_start);
    g_start = NULL;
}

int main(void) {
    QueryPerformanceFrequency(&g_freq);
    run_round("manual-reset event, one SetEvent:", TRUE);
    run_round("auto-reset event, one SetEvent:", FALSE);
    return 0;
}