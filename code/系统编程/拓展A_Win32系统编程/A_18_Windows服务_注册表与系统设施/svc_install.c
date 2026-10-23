/* 用服务控制管理器 API 安装、启动、停止、删除 MdrEcho 服务。
 *
 * 这些操作需要管理员权限，普通命令行会得到错误 5（拒绝访问）。
 * 构建：gcc -std=c17 -Wall -Wextra -o svc_install.exe svc_install.c
 * 用法：svc_install.exe install|start|stop|delete <exe 路径>
 */
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define SERVICE_NAME L"MdrEcho"

static void print_error(const wchar_t *what) {
    DWORD err = GetLastError();
    WCHAR *msg = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, 0, (LPWSTR)&msg, 0, NULL);
    wprintf(L"%ls failed: %lu: %ls", what, (unsigned long)err, msg != NULL ? msg : L"\n");
    LocalFree(msg);
}

static int do_install(SC_HANDLE scm, const wchar_t *exe) {
    SC_HANDLE svc = CreateServiceW(
        scm, SERVICE_NAME, L"Mdr Echo Service", SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, exe, NULL,
        NULL, NULL, L"NT AUTHORITY\\LocalService", NULL);

    if (svc == NULL) {
        print_error(L"CreateServiceW");
        return 1;
    }
    wprintf(L"service %ls installed, binary: %ls\n", SERVICE_NAME, exe);
    CloseServiceHandle(svc);
    return 0;
}

static int do_start(SC_HANDLE scm) {
    SC_HANDLE svc = OpenServiceW(scm, SERVICE_NAME, SERVICE_START);
    if (svc == NULL) {
        print_error(L"OpenServiceW");
        return 1;
    }
    if (!StartServiceW(svc, 0, NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING) {
            wprintf(L"service already running\n");
        } else {
            SetLastError(err);
            print_error(L"StartServiceW");
            CloseServiceHandle(svc);
            return 1;
        }
    } else {
        wprintf(L"service %ls started\n", SERVICE_NAME);
    }
    CloseServiceHandle(svc);
    return 0;
}

static int do_stop(SC_HANDLE scm) {
    SC_HANDLE svc = OpenServiceW(scm, SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS);
    SERVICE_STATUS status;

    if (svc == NULL) {
        print_error(L"OpenServiceW");
        return 1;
    }
    if (!ControlService(svc, SERVICE_CONTROL_STOP, &status)) {
        print_error(L"ControlService");
        CloseServiceHandle(svc);
        return 1;
    }
    wprintf(L"stop requested, state now %lu\n", (unsigned long)status.dwCurrentState);
    CloseServiceHandle(svc);
    return 0;
}

static int do_delete(SC_HANDLE scm) {
    SC_HANDLE svc = OpenServiceW(scm, SERVICE_NAME, DELETE);
    if (svc == NULL) {
        print_error(L"OpenServiceW");
        return 1;
    }
    if (!DeleteService(svc)) {
        print_error(L"DeleteService");
        CloseServiceHandle(svc);
        return 1;
    }
    wprintf(L"service %ls marked for deletion\n", SERVICE_NAME);
    CloseServiceHandle(svc);
    return 0;
}

int main(int argc, char *argv[]) {
    SC_HANDLE scm;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        wprintf(L"usage: %hs install|start|stop|delete [exe path]\n", argv[0]);
        return 2;
    }

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (scm == NULL) {
        print_error(L"OpenSCManagerW");
        return 1;
    }

    int rc;
    if (strcmp(argv[1], "install") == 0) {
        wchar_t exe[MAX_PATH];
        if (argc < 3) {
            wprintf(L"install needs the full path of echo_svc.exe\n");
            CloseServiceHandle(scm);
            return 2;
        }
        MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, exe, MAX_PATH);
        rc = do_install(scm, exe);
    } else if (strcmp(argv[1], "start") == 0) {
        rc = do_start(scm);
    } else if (strcmp(argv[1], "stop") == 0) {
        rc = do_stop(scm);
    } else if (strcmp(argv[1], "delete") == 0) {
        rc = do_delete(scm);
    } else {
        wprintf(L"unknown command: %hs\n", argv[1]);
        rc = 2;
    }

    CloseServiceHandle(scm);
    return rc;
}