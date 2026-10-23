/* orphan.c：启动 sleeper.exe 后立即退出，用来观察父进程结束后子进程的父 PID。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    wchar_t cmd[] = L"sleeper.exe 120";
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    printf("parent %lu started child %lu, exits now\n", (unsigned long)GetCurrentProcessId(),
           (unsigned long)pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}