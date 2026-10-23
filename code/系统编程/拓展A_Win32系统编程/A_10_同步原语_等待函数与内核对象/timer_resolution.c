#include <windows.h>
#include <stdio.h>

#define TICKS 6

static LARGE_INTEGER g_freq;

static double ms_since(LARGE_INTEGER t0) {
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - t0.QuadPart) * 1000.0 /
           (double)g_freq.QuadPart;
}

/* 用给定周期跑一轮，打印每次到期距上一次的间隔。 */
static void run_round(const char *label, DWORD flags, LONG period) {
    LARGE_INTEGER due;
    LARGE_INTEGER last;
    HANDLE timer;
    int i;

    timer = CreateWaitableTimerExW(NULL, NULL, flags, TIMER_ALL_ACCESS);
    if (timer == NULL) {
        printf("%s: CreateWaitableTimerExW failed: %lu\n", label,
               GetLastError());
        return;
    }
    due.QuadPart = -(LONGLONG)period * 10000LL; /* 相对时间，单位 100 ns */
    if (!SetWaitableTimer(timer, &due, period, NULL, NULL, FALSE)) {
        printf("%s: SetWaitableTimer failed: %lu\n", label, GetLastError());
        CloseHandle(timer);
        return;
    }

    printf("%s, period %ld ms:\n", label, period);
    QueryPerformanceCounter(&last);
    for (i = 0; i < TICKS; i++) {
        LARGE_INTEGER now;

        if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0) {
            break;
        }
        QueryPerformanceCounter(&now);
        printf("  tick %d: interval %6.2f ms\n", i + 1, ms_since(last));
        last = now;
    }
    CloseHandle(timer);
}

int main(void) {
    QueryPerformanceFrequency(&g_freq);
    run_round("no flag", 0, 5);
    run_round("HIGH_RESOLUTION", CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, 5);
    return 0;
}