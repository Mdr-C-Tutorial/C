#include <windows.h>
#include <stdio.h>

static volatile LONG g_apc_runs;
static HANDLE g_never; /* 永远不会被 SetEvent 的事件 */

static void CALLBACK apc_func(ULONG_PTR param) {
    LONG n = InterlockedIncrement(&g_apc_runs);

    printf("  APC #%ld ran in thread %lu, param = %llu\n", (long)n,
           (unsigned long)GetCurrentThreadId(), (unsigned long long)param);
}

static DWORD WINAPI worker(void *param) {
    DWORD r;

    (void)param;
    printf("worker: entering a non-alertable wait\n");
    r = WaitForSingleObject(g_never, 300);
    printf("worker: non-alertable wait returned %s\n",
           r == WAIT_TIMEOUT ? "WAIT_TIMEOUT" : "something else");

    printf("worker: entering an alertable wait\n");
    r = WaitForSingleObjectEx(g_never, 2000, TRUE);
    printf("worker: alertable wait returned %s, APCs run: %ld\n",
           r == WAIT_IO_COMPLETION
               ? "WAIT_IO_COMPLETION"
               : (r == WAIT_TIMEOUT ? "WAIT_TIMEOUT" : "other"),
           (long)g_apc_runs);

    /* 队列里还有别的 APC 时，每次可警报等待只返回一次 */
    if (r == WAIT_IO_COMPLETION) {
        r = WaitForSingleObjectEx(g_never, 2000, TRUE);
        printf("worker: second alertable wait returned %s, APCs run: %ld\n",
               r == WAIT_IO_COMPLETION
                   ? "WAIT_IO_COMPLETION"
                   : (r == WAIT_TIMEOUT ? "WAIT_TIMEOUT" : "other"),
               (long)g_apc_runs);
    }
    return 0;
}

int main(void) {
    HANDLE th;
    DWORD tid;

    g_never = CreateEventW(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, worker, NULL, 0, &tid);
    if (g_never == NULL || th == NULL) {
        printf("create failed: %lu\n", GetLastError());
        return 1;
    }
    printf("main: worker thread id %lu\n", (unsigned long)tid);
    Sleep(100);

    printf("main: queueing an APC while the worker is not alertable\n");
    QueueUserAPC(apc_func, th, 111);
    Sleep(400);
    printf("main: APCs run so far: %ld\n", (long)g_apc_runs);

    printf("main: queueing two more APCs\n");
    QueueUserAPC(apc_func, th, 222);
    QueueUserAPC(apc_func, th, 333);

    WaitForSingleObject(th, INFINITE);
    printf("main: worker exited, APCs run: %ld\n", (long)g_apc_runs);
    CloseHandle(th);
    CloseHandle(g_never);
    return 0;
}