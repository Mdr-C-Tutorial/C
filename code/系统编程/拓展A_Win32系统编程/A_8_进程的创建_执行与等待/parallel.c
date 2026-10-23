/* parallel.exe：并行启动 4 个 child.exe，等全部结束后逐个取退出码。 */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

#define N 4

int wmain(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    static const int codes[N] = {3, 1, 4, 1};
    HANDLE handles[N] = {0};
    PROCESS_INFORMATION procs[N] = {0};
    int started = 0;

    for (int i = 0; i < N; i++) {
        wchar_t cmd[256];

        _snwprintf(cmd, 256, L"child.exe %d job%d", codes[i], i);
        STARTUPINFOW si = {.cb = sizeof si};
        PROCESS_INFORMATION pi;
        if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            printf("CreateProcessW failed: %lu\n", GetLastError());
            break;
        }
        CloseHandle(pi.hThread);
        handles[i] = pi.hProcess;
        procs[i] = pi;
        started++;
    }
    if (started == 0) {
        return 1;
    }
    DWORD w = WaitForMultipleObjects((DWORD)started, handles, TRUE, INFINITE);

    if (w != WAIT_OBJECT_0) {
        printf("WaitForMultipleObjects -> %lu\n", (unsigned long)w);
    }
    for (int i = 0; i < started; i++) {
        DWORD code = 0;

        GetExitCodeProcess(handles[i], &code);
        printf("job %d (pid %lu): exit code %lu\n", i, (unsigned long)procs[i].dwProcessId,
               (unsigned long)code);
        CloseHandle(handles[i]);
    }
    return 0;
}