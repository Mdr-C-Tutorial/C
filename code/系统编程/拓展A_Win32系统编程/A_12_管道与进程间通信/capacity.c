/* 父进程建一根匿名管道给子进程做标准输出，先不读；2 秒后看子进程停在哪里。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

static void show_error(const char *what) {
    DWORD err = GetLastError();
    WCHAR *msg = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&msg, 0, NULL);
    printf("%s: error %lu: %ls", what, err, msg != NULL ? msg : L"");
    LocalFree(msg);
}

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        fwprintf(stderr, L"usage: capacity.exe child.exe log-file\n");
        return 2;
    }

    SECURITY_ATTRIBUTES sa = {.nLength = sizeof sa, .bInheritHandle = TRUE};
    HANDLE read_end = NULL;
    HANDLE write_end = NULL;
    if (!CreatePipe(&read_end, &write_end, &sa, 0)) {
        show_error("CreatePipe");
        return 1;
    }
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    wchar_t cmd[512];
    wcscpy(cmd, argv[1]);
    if (argc > 2) {
        wcscat(cmd, L" ");
        wcscat(cmd, argv[2]);
    }

    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = write_end;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        show_error("CreateProcessW");
        return 1;
    }
    CloseHandle(write_end);
    CloseHandle(pi.hThread);

    Sleep(2000);
    DWORD avail = 0;
    DWORD wait = WaitForSingleObject(pi.hProcess, 0);
    PeekNamedPipe(read_end, NULL, 0, NULL, &avail, NULL);
    wprintf(L"after 2 s: %lu bytes buffered, child %ls\n", avail,
            wait == WAIT_OBJECT_0 ? L"exited" : L"blocked in WriteFile");

    char buf[65536];
    DWORD got = 0;
    unsigned long long total = 0;
    while (ReadFile(read_end, buf, sizeof buf, &got, NULL) && got > 0) {
        total += got;
    }
    wprintf(L"drained %llu bytes\n", total);

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(read_end);
    return 0;
}
