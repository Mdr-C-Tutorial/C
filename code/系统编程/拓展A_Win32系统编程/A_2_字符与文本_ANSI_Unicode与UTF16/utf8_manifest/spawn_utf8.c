/* spawn_utf8.c：父进程带 UTF-8 清单，以它启动不带清单的 acp.exe，看代码页是否被继承。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("parent GetACP() = %u\n", GetACP());

    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    wchar_t cmd[] = L"acp.exe";
    if (!CreateProcessW(L"acp.exe", cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}