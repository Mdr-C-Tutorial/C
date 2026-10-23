/* run.exe：启动一个程序，原样传递参数，等待结束后报告退出码与 CPU 时间。
 * 用法：run.exe 程序 [参数...] */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

#define CMD_MAX 32768

static void print_error(const wchar_t *what) {
    DWORD err = GetLastError();
    wchar_t *msg = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, 0, (wchar_t *)&msg, 0, NULL);
    if (msg != NULL) {
        size_t n = wcslen(msg);
        while (n > 0 && (msg[n - 1] == L'\r' || msg[n - 1] == L'\n' || msg[n - 1] == L' ')) {
            msg[--n] = L'\0';
        }
    }
    fwprintf(stderr, L"%ls: error %lu: %ls\n", what, (unsigned long)err,
             msg != NULL ? msg : L"(没有可用的消息)");
    LocalFree(msg);
}

/* 追加一个字符，缓冲区不够时返回 0。 */
static int put(wchar_t *dst, size_t cap, size_t *len, wchar_t c) {
    if (*len + 1 >= cap) {
        return 0;
    }
    dst[(*len)++] = c;
    dst[*len] = L'\0';
    return 1;
}

/* 按子进程 CRT 的解析规则引用一个参数，追加到 dst 末尾。
 * 反斜杠只在双引号之前或参数末尾才需要成对。 */
static int append_quoted_arg(wchar_t *dst, size_t cap, size_t *len, const wchar_t *arg) {
    int quote = (*arg == L'\0');

    for (const wchar_t *p = arg; *p != L'\0'; p++) {
        if (*p == L' ' || *p == L'\t' || *p == L'\n' || *p == L'\v' || *p == L'"') {
            quote = 1;
        }
    }
    if (!quote) {
        for (const wchar_t *p = arg; *p != L'\0'; p++) {
            if (!put(dst, cap, len, *p)) {
                return 0;
            }
        }
        return 1;
    }

    if (!put(dst, cap, len, L'"')) {
        return 0;
    }
    const wchar_t *p = arg;
    while (*p != L'\0') {
        size_t backslashes = 0;

        while (*p == L'\\') {
            backslashes++;
            p++;
        }
        if (*p == L'\0') {
            /* 末尾的反斜杠成对，后面那个引号才是结束引号。 */
            for (size_t i = 0; i < backslashes * 2; i++) {
                if (!put(dst, cap, len, L'\\')) {
                    return 0;
                }
            }
        } else if (*p == L'"') {
            /* 反斜杠成对，再加一个转义这个引号。 */
            for (size_t i = 0; i < backslashes * 2 + 1; i++) {
                if (!put(dst, cap, len, L'\\')) {
                    return 0;
                }
            }
            if (!put(dst, cap, len, L'"')) {
                return 0;
            }
            p++;
        } else {
            for (size_t i = 0; i < backslashes; i++) {
                if (!put(dst, cap, len, L'\\')) {
                    return 0;
                }
            }
            if (!put(dst, cap, len, *p)) {
                return 0;
            }
            p++;
        }
    }
    return put(dst, cap, len, L'"');
}

static unsigned long long filetime_to_u64(FILETIME ft) {
    ULARGE_INTEGER u;

    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        fwprintf(stderr, L"用法：run.exe 程序 [参数...]\n");
        return 2;
    }

    wchar_t cmd[CMD_MAX] = L"";
    size_t len = 0;

    for (int i = 1; i < argc; i++) {
        if (i > 1 && !put(cmd, CMD_MAX, &len, L' ')) {
            fwprintf(stderr, L"命令行太长\n");
            return 2;
        }
        if (!append_quoted_arg(cmd, CMD_MAX, &len, argv[i])) {
            fwprintf(stderr, L"命令行太长\n");
            return 2;
        }
    }

    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        print_error(L"CreateProcessW");
        return 1;
    }
    CloseHandle(pi.hThread);

    if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
        print_error(L"WaitForSingleObject");
        CloseHandle(pi.hProcess);
        return 1;
    }

    DWORD code = 0;
    FILETIME creation = {0};
    FILETIME exit_time = {0};
    FILETIME kernel = {0};
    FILETIME user = {0};

    if (!GetExitCodeProcess(pi.hProcess, &code)) {
        print_error(L"GetExitCodeProcess");
    }
    if (!GetProcessTimes(pi.hProcess, &creation, &exit_time, &kernel, &user)) {
        print_error(L"GetProcessTimes");
    }
    DWORD pid = pi.dwProcessId;

    CloseHandle(pi.hProcess);

    double elapsed = (double)(filetime_to_u64(exit_time) - filetime_to_u64(creation)) / 10000.0;
    printf("pid: %lu\n", (unsigned long)pid);
    printf("exit code: %lu\n", (unsigned long)code);
    printf("elapsed: %.1f ms, kernel: %.1f ms, user: %.1f ms\n", elapsed,
           (double)filetime_to_u64(kernel) / 10000.0,
           (double)filetime_to_u64(user) / 10000.0);
    return (int)code;
}