#include <windows.h>
#include <stdio.h>

#define DOWNLOADS 5
#define SLOTS 2

static HANDLE g_slots;    /* 计数信号量：空闲槽位 */
static HANDLE g_finished; /* 全部完成时置位 */
static volatile LONG g_running;
static volatile LONG g_left;
static LARGE_INTEGER g_t0;
static LARGE_INTEGER g_freq;

static double ms_since_start(void) {
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - g_t0.QuadPart) * 1000.0 /
           (double)g_freq.QuadPart;
}

struct job {
    int id;
};

static DWORD WINAPI download(void *param) {
    struct job *job = param;
    DWORD r = WaitForSingleObject(g_slots, INFINITE);

    if (r != WAIT_OBJECT_0) {
        printf("%7.1f ms  job %d  wait returned %lu\n", ms_since_start(),
               job->id, (unsigned long)r);
        return 1;
    }
    printf("%7.1f ms  job %d  start  (%ld running)\n", ms_since_start(),
           job->id, (long)InterlockedIncrement(&g_running));

    Sleep(200 + (DWORD)job->id * 100);

    printf("%7.1f ms  job %d  done   (%ld running)\n", ms_since_start(),
           job->id, (long)InterlockedDecrement(&g_running));
    ReleaseSemaphore(g_slots, 1, NULL);

    if (InterlockedDecrement(&g_left) == 0) {
        SetEvent(g_finished);
    }
    return 0;
}

int main(void) {
    struct job jobs[DOWNLOADS];
    HANDLE threads[DOWNLOADS];
    DWORD r;
    int i;

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_t0);

    g_slots = CreateSemaphoreW(NULL, SLOTS, SLOTS, NULL);
    g_finished = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_slots == NULL || g_finished == NULL) {
        printf("create failed: %lu\n", GetLastError());
        return 1;
    }
    g_left = DOWNLOADS;

    for (i = 0; i < DOWNLOADS; i++) {
        jobs[i].id = i + 1;
        threads[i] = CreateThread(NULL, 0, download, &jobs[i], 0, NULL);
    }

    r = WaitForSingleObject(g_finished, 5000);
    printf("%7.1f ms  main: all done (%s)\n", ms_since_start(),
           r == WAIT_OBJECT_0 ? "event signaled" : "timed out");

    for (i = 0; i < DOWNLOADS; i++) {
        CloseHandle(threads[i]);
    }
    CloseHandle(g_slots);
    CloseHandle(g_finished);
    return 0;
}