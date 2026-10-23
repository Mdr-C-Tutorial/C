/* 反例：先等子进程结束再读管道，输出一多就双方一起卡住。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        return 2;
    }
    size_t len = 1;
    for (int i = 1; i < argc; i++) {
        len += wcslen(argv[i]) + 1;
    }
    wchar_t *cmd = malloc(len * sizeof *cmd);
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
    CloseHandle(write_end);

    /* 错在这里：子进程还没写完，父进程就等它结束。 */
    wprintf(L"waiting for the child before reading...\n");
    fflush(stdout);
    DWORD wait = WaitForSingleObject(pi.hProcess, 3000);
    wprintf(L"WaitForSingleObject = %lu (%s)\n", wait,
            wait == WAIT_TIMEOUT ? L"WAIT_TIMEOUT" : L"signaled");

    char buf[4096];
    DWORD got = 0;
    ReadFile(read_end, buf, sizeof buf, &got, NULL);
    wprintf(L"read %lu bytes\n", got);

    CloseHandle(pi.hProcess);
    CloseHandle(read_end);
    free(cmd);
    return 0;
}