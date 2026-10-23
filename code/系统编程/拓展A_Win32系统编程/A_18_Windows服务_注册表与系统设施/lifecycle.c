/* 服务的四个状态与两个报告字段：把将要交给 SetServiceStatus 的内容打印出来。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o lifecycle.exe lifecycle.c
 */
#include <windows.h>
#include <stdio.h>

static const char *state_name(DWORD state) {
    switch (state) {
    case SERVICE_STOPPED:
        return "SERVICE_STOPPED";
    case SERVICE_START_PENDING:
        return "SERVICE_START_PENDING";
    case SERVICE_RUNNING:
        return "SERVICE_RUNNING";
    case SERVICE_STOP_PENDING:
        return "SERVICE_STOP_PENDING";
    default:
        return "(other)";
    }
}

static void report(DWORD state, DWORD accepted, DWORD checkpoint, DWORD wait_hint) {
    printf("%-22s controls 0x%02lX checkpoint %lu wait_hint %lu ms\n", state_name(state),
           (unsigned long)accepted, (unsigned long)checkpoint, (unsigned long)wait_hint);
}

int main(void) {
    DWORD check = 1;

    /* 初始化可能要一段时间：每次报告 PENDING 都把 checkpoint 加一，并给出预计剩余时间。 */
    report(SERVICE_START_PENDING, 0, check++, 3000);
    Sleep(100);
    report(SERVICE_START_PENDING, 0, check++, 3000);

    /* 进入 RUNNING 时才声明愿意接收哪些控制。 */
    report(SERVICE_RUNNING, SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN, 0, 0);

    /* 收到 STOP 之后先报告 STOP_PENDING，做完清理再报告 STOPPED。 */
    report(SERVICE_STOP_PENDING, 0, check, 5000);
    Sleep(100);
    report(SERVICE_STOPPED, 0, 0, 0);
    return 0;
}