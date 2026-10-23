/* quote_demo.c：把若干参数分别用「简单加引号」和正确规则拼成命令行，
 * 启动 child.exe，看子进程实际收到的参数。 */
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

/* 简单做法：含空白就整段加引号，不处理反斜杠和引号。 */
static void naive_quote(wchar_t *dst, size_t cap, const wchar_t *src) {
    int need = (wcschr(src, L' ') != NULL);

    if (need) {
        _snwprintf(dst, cap, L"\"%ls\"", src);
    } else {
        _snwprintf(dst, cap, L"%ls", src);
    }
}

static int put(wchar_t *dst, size_t cap, size_t *len, wchar_t c) {
    if (*len + 1 >= cap) {
        return 0;
    }
    dst[(*len)++] = c;
    dst[*len] = L'\0';
    return 1;
}

/* 按 MSVC CRT 的解析规则反向引用一个参数（Everyone quotes command line
 * arguments the wrong way 里 ArgvQuote 的 C 版）。 */
static int append_quoted_arg(wchar_t *dst, size_t cap, size_t *len, const wchar_t *arg) {
    int quote = (*arg == L'\0');

    for (const wchar_t *p = arg; *p != L'\0'; p++) {
        if (*p == L' ' || *p == L'\t' || *p == L'"') {
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
        size_t slashes = 0;
        while (*p == L'\\') {
            slashes++;
            p++;
        }
        if (*p == L'\0') {
            for (size_t i = 0; i < slashes * 2; i++) {
                if (!put(dst, cap, len, L'\\')) {
                    return 0;
                }
            }
        } else if (*p == L'"') {
            for (size_t i = 0; i < slashes * 2 + 1; i++) {
                if (!put(dst, cap, len, L'\\')) {
                    return 0;
                }
            }
            if (!put(dst, cap, len, L'"')) {
                return 0;
            }
            p++;
        } else {
            for (size_t i = 0; i < slashes; i++) {
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

static void run_and_report(const wchar_t *label, wchar_t *cmd) {
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;

    wprintf(L"--- %ls\n    %ls\n", label, cmd);
    fflush(stdout);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        print_error(L"CreateProcessW");
        return;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    static const wchar_t *args[] = {L"child.exe", L"0", L"a b", L"say \"hi\"",
                                    L"C:\\dir with space\\", L"tail"};
    int n = (int)(sizeof args / sizeof args[0]);

    wchar_t naive[CMD_MAX] = L"";
    size_t nlen = 0;
    for (int i = 0; i < n; i++) {
        wchar_t one[CMD_MAX];
        if (i > 0) {
            put(naive, CMD_MAX, &nlen, L' ');
        }
        naive_quote(one, CMD_MAX, args[i]);
        for (const wchar_t *p = one; *p != L'\0'; p++) {
            put(naive, CMD_MAX, &nlen, *p);
        }
    }
    wchar_t naive_copy[CMD_MAX];
    wcscpy(naive_copy, naive);
    run_and_report(L"简单加引号", naive_copy);

    wchar_t good[CMD_MAX] = L"";
    size_t glen = 0;
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            put(good, CMD_MAX, &glen, L' ');
        }
        if (!append_quoted_arg(good, CMD_MAX, &glen, args[i])) {
            fwprintf(stderr, L"命令行太长\n");
            return 1;
        }
    }
    run_and_report(L"正确引用", good);
    return 0;
}