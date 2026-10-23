/* ctrl_handler.c —— 控制处理函数在系统新建的线程中运行。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o ctrl_handler.exe ctrl_handler.c
 * 运行：ctrl_handler.exe，然后按 Ctrl+C。
 */
#include <windows.h>
#include <stdio.h>

static HANDLE g_stop;
static DWORD g_main_thread;

static BOOL WINAPI on_control(DWORD type) {
    printf("[handler] type=%lu  handler thread=%lu  main thread=%lu\n", (unsigned long)type,
           (unsigned long)GetCurrentThreadId(), (unsigned long)g_main_thread);
    fflush(stdout);

    /* 处理函数里只做最少的事：通知主循环，由主循环收尾。 */
    SetEvent(g_stop);
    return TRUE;
}

int main(void) {
    g_main_thread = GetCurrentThreadId();
    g_stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_stop == NULL) {
        printf("CreateEventW failed: %lu\n", GetLastError());
        return 1;
    }
    if (!SetConsoleCtrlHandler(on_control, TRUE)) {
        printf("SetConsoleCtrlHandler failed: %lu\n", GetLastError());
        return 1;
    }

    printf("main thread %lu, press Ctrl+C\n", (unsigned long)g_main_thread);
    fflush(stdout);

    DWORD r = WaitForSingleObject(g_stop, 20000);
    if (r == WAIT_OBJECT_0) {
        printf("main: stop event is set, cleaning up\n");
    } else {
        printf("main: no control event, wait result %lu\n", (unsigned long)r);
    }

    SetConsoleCtrlHandler(on_control, FALSE);
    CloseHandle(g_stop);
    return 0;
}