/* mini_ps.exe：用 CreateToolhelp32Snapshot 列出进程的 PID、父 PID 与映像名。
 * 不带参数时只列前 8 行和自身；带参数时列出映像名包含该子串的进程。 */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <tlhelp32.h>

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snap == INVALID_HANDLE_VALUE) {
        printf("CreateToolhelp32Snapshot failed: %lu\n", GetLastError());
        return 1;
    }

    PROCESSENTRY32W pe = {.dwSize = sizeof pe};
    int count = 0;
    DWORD self = GetCurrentProcessId();
    const wchar_t *filter = argc > 1 ? argv[1] : NULL;

    wprintf(L"   PID   PPID  image\n");
    if (Process32FirstW(snap, &pe)) {
        do {
            int match = filter != NULL ? (wcsstr(pe.szExeFile, filter) != NULL)
                                       : (count < 8 || pe.th32ProcessID == self);
            if (match) {
                wprintf(L"%6lu %6lu  %ls\n", (unsigned long)pe.th32ProcessID,
                        (unsigned long)pe.th32ParentProcessID, pe.szExeFile);
            }
            count++;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (filter == NULL) {
        wprintf(L"-- %d processes; self is %lu\n", count, (unsigned long)self);
    }
    return 0;
}