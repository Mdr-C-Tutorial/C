/* cp_literal.c：把字符串字面量直接交给 CreateProcessW。
 * CreateProcessW 会就地修改命令行缓冲区，字面量位于只读数据段。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, L"child.exe 7 hello", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;

    GetExitCodeProcess(pi.hProcess, &code);
    printf("exit code: %lu\n", (unsigned long)code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}