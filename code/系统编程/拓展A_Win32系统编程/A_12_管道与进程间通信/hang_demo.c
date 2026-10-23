/* 反例：父进程忘了关闭自己手里的写端，读端永远等不到结束。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

#define BUF_SIZE 4096

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        fwprintf(stderr, L"usage: hang_demo.exe command [args...]\n");
        return 2;
    }

    size_t len = 1;
    for (int i = 1; i < argc; i++) {
        len += wcslen(argv[i]) + 1;
    }
    wchar_t *cmd = malloc(len * sizeof *cmd);
    if (cmd == NULL) {
        return 2;
    }
    cmd[0] = L'\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            wcscat(cmd, L" ");
        }
        wcscat(cmd, argv[i]);
    }

    SECURITY_ATTRIBUTES sa = {.nLength = sizeof sa, .bInheritHandle = TRUE};
    HANDLE read_end = NULL;
    HANDLE write_end = NULL;

    if (!CreatePipe(&read_end, &write_end, &sa, 0)) {
        wprintf(L"CreatePipe: error %lu\n", GetLastError());
        return 1;
    }
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = write_end;
    si.hStdError = write_end;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        wprintf(L"CreateProcessW: error %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(pi.hThread);

    /* 这里漏掉了 CloseHandle(write_end)：写端仍有一个句柄开着，
       管道就永远不会报告「所有写端已关闭」。 */
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    char buf[BUF_SIZE];
    DWORD got = 0;
    while (ReadFile(read_end, buf, sizeof buf, &got, NULL) && got > 0) {
        DWORD written = 0;
        WriteFile(out, buf, got, &written, NULL);
    }
    wprintf(L"reader returned, last error %lu\n", GetLastError());

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    wprintf(L"child exit code %lu\n", code);

    CloseHandle(pi.hProcess);
    CloseHandle(write_end);
    CloseHandle(read_end);
    free(cmd);
    return 0;
}