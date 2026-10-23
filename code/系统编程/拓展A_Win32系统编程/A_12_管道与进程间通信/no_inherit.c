/* 反例：CreatePipe 时没设 bInheritHandle，子进程拿不到可用的标准输出。 */
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

    /* 这里给的安全属性是 NULL：两个句柄都不可继承。 */
    HANDLE read_end = NULL;
    HANDLE write_end = NULL;
    if (!CreatePipe(&read_end, &write_end, NULL, 0)) {
        wprintf(L"CreatePipe: error %lu\n", GetLastError());
        return 1;
    }

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

    char buf[4096];
    DWORD got = 0;
    BOOL ok = ReadFile(read_end, buf, sizeof buf, &got, NULL);
    wprintf(L"first ReadFile: ok=%d, %lu bytes, error=%lu\n", ok, got, ok ? 0 : GetLastError());

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    wprintf(L"child exit code %lu\n", code);

    CloseHandle(pi.hProcess);
    CloseHandle(read_end);
    free(cmd);
    return 0;
}