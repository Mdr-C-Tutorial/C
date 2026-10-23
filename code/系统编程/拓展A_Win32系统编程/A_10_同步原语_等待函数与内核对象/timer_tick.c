#include <windows.h>
#include <stdio.h>

#define TICKS 5

static LARGE_INTEGER g_freq;
static LARGE_INTEGER g_start;

static double ms_since_start(void) {
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - g_start.QuadPart) * 1000.0 /
           (double)g_freq.QuadPart;
}

int main(void) {
    LARGE_INTEGER due;
    HANDLE timer;
    double last = 0.0;
    int i;

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);

    timer = CreateWaitableTimerExW(NULL, NULL,
                                   CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                   TIMER_ALL_ACCESS);
    if (timer == NULL) {
        printf("CreateWaitableTimerExW failed: %lu\n", GetLastError());
        return 1;
    }

    /* 负值表示相对时间，单位 100 ns：-2 000 000 即 200 毫秒后首次到期 */
    due.QuadPart = -2000000LL;
    if (!SetWaitableTimer(timer, &due, 200, NULL, NULL, FALSE)) {
        printf("SetWaitableTimer failed: %lu\n", GetLastError());
        CloseHandle(timer);
        return 1;
    }

    for (i = 0; i < TICKS; i++) {
        DWORD r = WaitForSingleObject(timer, INFINITE);
        double now = ms_since_start();

        if (r != WAIT_OBJECT_0) {
            printf("wait %d returned %lu\n", i, (unsigned long)r);
            break;
        }
        printf("tick %d at %7.1f ms, interval %6.1f ms\n", i + 1, now,
               i == 0 ? now : now - last);
        last = now;
    }

    CloseHandle(timer);
    return 0;
}