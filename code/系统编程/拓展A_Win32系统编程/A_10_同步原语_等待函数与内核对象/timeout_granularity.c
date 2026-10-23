#include <windows.h>
#include <stdio.h>

/* 在一个永远不会被置位的事件上等待，测量超时的实际长度。 */

static LARGE_INTEGER g_freq;

static double ms_since(LARGE_INTEGER t0) {
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - t0.QuadPart) * 1000.0 /
           (double)g_freq.QuadPart;
}

static void probe(HANDLE never, DWORD ms) {
    LARGE_INTEGER t0;
    DWORD r;
    double elapsed;

    QueryPerformanceCounter(&t0);
    r = WaitForSingleObject(never, ms);
    elapsed = ms_since(t0);
    printf("timeout %3lu ms -> %-12s actual %6.2f ms\n", (unsigned long)ms,
           r == WAIT_TIMEOUT ? "WAIT_TIMEOUT" : "other", elapsed);
}

int main(void) {
    HANDLE never;
    DWORD requested[] = {0, 1, 5, 10, 16, 100};
    int i;

    QueryPerformanceFrequency(&g_freq);
    never = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (never == NULL) {
        printf("CreateEventW failed: %lu\n", GetLastError());
        return 1;
    }

    for (i = 0; i < (int)(sizeof requested / sizeof requested[0]); i++) {
        probe(never, requested[i]);
    }

    CloseHandle(never);
    return 0;
}