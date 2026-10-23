/* 只调用一次 StartServiceCtrlDispatcherW，看从命令行直接运行时得到什么。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o svc_probe.exe svc_probe.c
 */
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

static void WINAPI svc_main(DWORD argc, LPWSTR *argv) {
    (void)argc;
    (void)argv;
}

int main(void) {
    SERVICE_TABLE_ENTRYW table[] = {
        {L"MdrProbe", svc_main},
        {NULL, NULL},
    };
    WCHAR *msg = NULL;
    DWORD err;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (!StartServiceCtrlDispatcherW(table)) {
        err = GetLastError();
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, err, 0, (LPWSTR)&msg, 0, NULL);
        wprintf(L"StartServiceCtrlDispatcherW failed: %lu\n", (unsigned long)err);
        wprintf(L"message: %ls", msg != NULL ? msg : L"(none)\n");
        LocalFree(msg);
        return 1;
    }
    return 0;
}