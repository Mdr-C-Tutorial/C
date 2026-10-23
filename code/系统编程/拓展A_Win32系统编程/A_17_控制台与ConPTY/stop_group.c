/* stop_group.c —— 用 CREATE_NEW_PROCESS_GROUP + CTRL_BREAK_EVENT 停止子进程。
 *
 * 同一个可执行文件既是父进程也是子进程：带 --child 参数时扮演子进程。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o stop_group.exe stop_group.c
 * 运行：stop_group.exe
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int run_child(void) {
    /* 子进程不装控制处理函数，用默认处理程序：收到 CTRL_BREAK 就以 0xC000013A 结束。 */
    printf("child %lu started, sleeping\n", (unsigned long)GetCurrentProcessId());
    fflush(stdout);
    Sleep(10000);
    printf("child: slept the whole time, no control event arrived\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--child") == 0) {
        return run_child();
    }

    wchar_t exe[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe, MAX_PATH) == 0) {
        printf("GetModuleFileNameW failed: %lu\n", GetLastError());
        return 1;
    }
    wchar_t cmd[MAX_PATH + 32];
    swprintf(cmd, MAX_PATH + 32, L"\"%ls\" --child", exe);

    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    printf("parent %lu started child %lu in its own process group\n", (unsigned long)GetCurrentProcessId(),
           (unsigned long)pi.dwProcessId);

    Sleep(800);
    /* CTRL_C_EVENT 不能只发给某个进程组，只能用 CTRL_BREAK_EVENT。 */
    if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId)) {
        printf("GenerateConsoleCtrlEvent failed: %lu\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
    }
    printf("parent sent CTRL_BREAK_EVENT to group %lu\n", (unsigned long)pi.dwProcessId);

    WaitForSingleObject(pi.hProcess, 5000);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    printf("child exit code %lu (0x%08lX)\n", (unsigned long)code, (unsigned long)code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}