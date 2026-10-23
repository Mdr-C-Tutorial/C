/* 用 QueryServiceStatusEx 查询一个服务的状态：只读，不需要管理员权限。 */
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

static const wchar_t *state_name(DWORD state) {
    switch (state) {
    case SERVICE_STOPPED:
        return L"SERVICE_STOPPED";
    case SERVICE_START_PENDING:
        return L"SERVICE_START_PENDING";
    case SERVICE_STOP_PENDING:
        return L"SERVICE_STOP_PENDING";
    case SERVICE_RUNNING:
        return L"SERVICE_RUNNING";
    case SERVICE_CONTINUE_PENDING:
        return L"SERVICE_CONTINUE_PENDING";
    case SERVICE_PAUSE_PENDING:
        return L"SERVICE_PAUSE_PENDING";
    case SERVICE_PAUSED:
        return L"SERVICE_PAUSED";
    default:
        return L"(unknown)";
    }
}

int main(int argc, char *argv[]) {
    const wchar_t *name = argc > 1 ? NULL : L"EventLog";
    wchar_t wide_name[64];
    SC_HANDLE scm;
    SC_HANDLE svc;
    SERVICE_STATUS_PROCESS status;
    DWORD needed = 0;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc > 1) {
        MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, wide_name, 64);
        name = wide_name;
    }

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm == NULL) {
        wprintf(L"OpenSCManagerW failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }

    svc = OpenServiceW(scm, name, SERVICE_QUERY_STATUS);
    if (svc == NULL) {
        wprintf(L"OpenServiceW(%ls) failed: %lu\n", name, (unsigned long)GetLastError());
        CloseServiceHandle(scm);
        return 1;
    }

    if (!QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof status,
                              &needed)) {
        wprintf(L"QueryServiceStatusEx failed: %lu\n", (unsigned long)GetLastError());
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        return 1;
    }

    wprintf(L"service    : %ls\n", name);
    wprintf(L"state      : %lu (%ls)\n", (unsigned long)status.dwCurrentState,
            state_name(status.dwCurrentState));
    wprintf(L"type       : 0x%08lX\n", (unsigned long)status.dwServiceType);
    wprintf(L"pid        : %lu\n", (unsigned long)status.dwProcessId);
    wprintf(L"controls   : 0x%08lX\n", (unsigned long)status.dwControlsAccepted);
    wprintf(L"exit code  : %lu\n", (unsigned long)status.dwWin32ExitCode);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}