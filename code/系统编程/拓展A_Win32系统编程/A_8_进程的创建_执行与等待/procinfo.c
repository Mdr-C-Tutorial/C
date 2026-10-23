/* procinfo.c：由 PID 打开进程，取映像路径与 CPU 时间。
 * 用法：procinfo.exe PID */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

static unsigned long long filetime_to_u64(FILETIME f) {
    ULARGE_INTEGER u;

    u.LowPart = f.dwLowDateTime;
    u.HighPart = f.dwHighDateTime;
    return u.QuadPart;
}

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        wprintf(L"用法：procinfo.exe PID\n");
        return 2;
    }
    DWORD pid = (DWORD)_wtoi(argv[1]);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

    if (h == NULL) {
        printf("OpenProcess failed: %lu\n", GetLastError());
        return 1;
    }

    wchar_t path[MAX_PATH];
    DWORD n = MAX_PATH;

    if (QueryFullProcessImageNameW(h, 0, path, &n)) {
        wprintf(L"image: %ls\n", path);
    } else {
        printf("QueryFullProcessImageNameW failed: %lu\n", GetLastError());
    }

    FILETIME creation = {0};
    FILETIME exit_time = {0};
    FILETIME kernel = {0};
    FILETIME user = {0};

    if (!GetProcessTimes(h, &creation, &exit_time, &kernel, &user)) {
        printf("GetProcessTimes failed: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    wprintf(L"kernel %.1f ms, user %.1f ms\n", (double)filetime_to_u64(kernel) / 10000.0,
            (double)filetime_to_u64(user) / 10000.0);
    CloseHandle(h);
    return 0;
}