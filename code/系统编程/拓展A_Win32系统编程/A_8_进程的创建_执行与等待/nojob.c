/* nojob.c：只启动子进程就退出，不用作业对象。子进程会一直留着。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    wchar_t cmd[] = L"sleeper.exe 60";
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    printf("started %lu, parent exits now\n", (unsigned long)pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}