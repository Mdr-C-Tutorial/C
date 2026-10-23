/* spawn.c：最小示例——启动 child.exe，等它结束，取退出码。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    wchar_t cmd[] = L"child.exe 7 hello";
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD code = 0;

    GetExitCodeProcess(pi.hProcess, &code);
    printf("exit code: %lu\n", (unsigned long)code);
    CloseHandle(pi.hProcess);
    return 0;
}