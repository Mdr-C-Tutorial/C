/* 线程池等待对象：先只登记一次，连续 3 次 SetEvent 只有第 1 次触发回调；
 * 再改成每次 SetEvent 之前重新登记，3 次都触发。 */
#include <stdio.h>
#include <windows.h>

#define ROUNDS 3

static volatile LONG g_count = 0;

static VOID CALLBACK wait_cb(PTP_CALLBACK_INSTANCE instance, PVOID context,
                             PTP_WAIT wait, TP_WAIT_RESULT result) {
    LONG n = InterlockedIncrement(&g_count);

    (void)instance;
    (void)wait;
    (void)context;
    printf("  第 %ld 次回调：线程 %lu，result = %lu\n", n, GetCurrentThreadId(),
           (unsigned long)result);
}

static void run_case(HANDLE ev, PTP_WAIT wait, int rearm) {
    LONG before = g_count;

    for (int i = 0; i < ROUNDS; i++) {
        if (rearm) {
            SetThreadpoolWait(wait, ev, NULL);
        }
        Sleep(80);
        SetEvent(ev);
        Sleep(120);
    }
    printf("  本轮共回调 %ld 次\n", (long)(g_count - before));
}

int main(void) {
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    PTP_WAIT wait = CreateThreadpoolWait(wait_cb, NULL, NULL);

    if (wait == NULL) {
        printf("CreateThreadpoolWait 失败: %lu\n", GetLastError());
        return 1;
    }

    printf("只登记一次，之后连续 3 次 SetEvent：\n");
    SetThreadpoolWait(wait, ev, NULL);
    run_case(ev, wait, 0);

    printf("每次 SetEvent 前重新登记：\n");
    run_case(ev, wait, 1);

    /* 取消登记并等待正在运行的回调结束 */
    SetThreadpoolWait(wait, NULL, NULL);
    WaitForThreadpoolWaitCallbacks(wait, TRUE);
    CloseThreadpoolWait(wait);
    CloseHandle(ev);
    printf("结束，共 %ld 次回调\n", (long)g_count);
    return 0;
}