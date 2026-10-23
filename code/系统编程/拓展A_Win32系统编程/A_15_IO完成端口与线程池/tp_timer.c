/* 线程池计时器：每 300 ms 触发一次，共 5 次。
 * FILETIME 为负值表示相对时间，单位是 100 ns。 */
#include <stdio.h>
#include <windows.h>

#define TICKS 5

static volatile LONG g_count = 0;
static ULONGLONG g_start;

static VOID CALLBACK timer_cb(PTP_CALLBACK_INSTANCE instance, PVOID context,
                              PTP_TIMER timer) {
    LONG n;

    (void)instance;
    (void)timer;
    n = InterlockedIncrement(&g_count);
    printf("第 %ld 次触发：线程 %lu，距开始 %llu ms\n", n, GetCurrentThreadId(),
           (unsigned long long)(GetTickCount64() - g_start));
    if (n == TICKS) {
        SetEvent((HANDLE)context);
    }
}

int main(void) {
    HANDLE done = CreateEventW(NULL, TRUE, FALSE, NULL);
    FILETIME due;
    ULARGE_INTEGER li;
    PTP_TIMER timer;

    g_start = GetTickCount64();

    /* 相对时间：负的 100 ns 计数表示从现在起 300 ms */
    li.QuadPart = -(LONGLONG)300 * 10000;
    due.dwLowDateTime = li.LowPart;
    due.dwHighDateTime = li.HighPart;

    timer = CreateThreadpoolTimer(timer_cb, done, NULL);
    if (timer == NULL) {
        printf("CreateThreadpoolTimer 失败: %lu\n", GetLastError());
        return 1;
    }

    /* 周期 300 ms；第二个参数为 0 表示不取消，也没有窗口宽度 */
    SetThreadpoolTimer(timer, &due, 300, 0);

    WaitForSingleObject(done, 5000);

    SetThreadpoolTimer(timer, NULL, 0, 0);
    WaitForThreadpoolTimerCallbacks(timer, TRUE);
    CloseThreadpoolTimer(timer);
    CloseHandle(done);
    printf("计时器已关闭，共触发 %ld 次\n", (long)g_count);
    return 0;
}