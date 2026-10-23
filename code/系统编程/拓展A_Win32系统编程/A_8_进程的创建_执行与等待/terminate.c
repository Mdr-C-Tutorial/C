/* terminate.c：正在运行的进程退出码是 STILL_ACTIVE；
 * TerminateProcess 指定退出码，且不运行子进程的 atexit 处理程序。 */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    wchar_t cmd[] = L"cleanup_child.exe 30";
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(pi.hThread);

    DWORD code = 0;

    GetExitCodeProcess(pi.hProcess, &code);
    printf("running: exit code %lu (STILL_ACTIVE is %ld)\n", (unsigned long)code,
           (long)STILL_ACTIVE);
    Sleep(500);
    TerminateProcess(pi.hProcess, 42);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    printf("after TerminateProcess(42): exit code %lu\n", (unsigned long)code);
    CloseHandle(pi.hProcess);
    return 0;
}