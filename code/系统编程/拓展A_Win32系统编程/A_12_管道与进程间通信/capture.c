/* 用匿名管道捕获子进程的标准输出与标准错误。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

#define BUF_SIZE 4096

static void print_error(const wchar_t *what) {
    DWORD err = GetLastError();
    WCHAR *msg = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&msg, 0, NULL);
    fwprintf(stderr, L"%ls: error %lu: %ls", what, err, msg != NULL ? msg : L"");
    LocalFree(msg);
}

/* 把参数用空格拼成命令行；引号规则见 A.8。 */
static wchar_t *build_command_line(int argc, wchar_t *argv[]) {
    size_t len = 1;

    for (int i = 1; i < argc; i++) {
        len += wcslen(argv[i]) + 1;
    }
    wchar_t *cmd = malloc(len * sizeof *cmd);
    if (cmd == NULL) {
        return NULL;
    }
    cmd[0] = L'\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            wcscat(cmd, L" ");
        }
        wcscat(cmd, argv[i]);
    }
    return cmd;
}

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        fwprintf(stderr, L"usage: capture.exe command [args...]\n");
        return 2;
    }

    wchar_t *cmd = build_command_line(argc, argv);
    if (cmd == NULL) {
        fwprintf(stderr, L"out of memory\n");
        return 2;
    }

    SECURITY_ATTRIBUTES sa = {.nLength = sizeof sa, .bInheritHandle = TRUE};
    HANDLE read_end = NULL;
    HANDLE write_end = NULL;
    HANDLE process = NULL;
    HANDLE thread = NULL;
    int status = 1;

    if (!CreatePipe(&read_end, &write_end, &sa, 0)) {
        print_error(L"CreatePipe");
        goto cleanup;
    }
    /* 父进程这一端不该被子进程继承。 */
    if (!SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0)) {
        print_error(L"SetHandleInformation");
        goto cleanup;
    }

    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = write_end;
    si.hStdError = write_end;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        print_error(L"CreateProcessW");
        goto cleanup;
    }
    process = pi.hProcess;
    thread = pi.hThread;

    /* 关键一步：子进程已经拿到写端的副本，父进程立刻关掉自己这一份。 */
    CloseHandle(write_end);
    write_end = NULL;

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    unsigned long long total = 0;
    unsigned long long lines = 0;
    char buf[BUF_SIZE];

    for (;;) {
        DWORD got = 0;
        if (!ReadFile(read_end, buf, sizeof buf, &got, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE) {
                break; /* 所有写端都已关闭 */
            }
            fwprintf(stderr, L"ReadFile: error %lu\n", err);
            goto cleanup;
        }
        DWORD written = 0;
        WriteFile(out, buf, got, &written, NULL);
        total += got;
        for (DWORD i = 0; i < got; i++) {
            if (buf[i] == '\n') {
                lines++;
            }
        }
    }

    WaitForSingleObject(process, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process, &code);
    printf("-- captured %llu bytes, %llu lines, exit code %lu\n", total, lines, code);
    status = 0;

cleanup:
    if (read_end != NULL) {
        CloseHandle(read_end);
    }
    if (write_end != NULL) {
        CloseHandle(write_end);
    }
    if (thread != NULL) {
        CloseHandle(thread);
    }
    if (process != NULL) {
        CloseHandle(process);
    }
    free(cmd);
    return status;
}