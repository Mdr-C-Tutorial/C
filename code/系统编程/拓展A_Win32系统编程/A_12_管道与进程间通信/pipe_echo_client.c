/* 命名管道客户端：把每个参数作为一条 UTF-8 消息发送，并读回回声。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_echo"
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

/* 把宽字符串转成 UTF-8，返回 malloc 的缓冲区。 */
static char *to_utf8(const wchar_t *text, int *len) {
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char *bytes = malloc((size_t)n);

    if (bytes != NULL) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, bytes, n, NULL, NULL);
        *len = n - 1; /* 不发送结尾空字符 */
    }
    return bytes;
}

int wmain(int argc, wchar_t *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        fwprintf(stderr, L"usage: pipe_echo_client.exe message [message...]\n");
        return 2;
    }

    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (;;) {
        pipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, 0, NULL);
        if (pipe != INVALID_HANDLE_VALUE) {
            break;
        }
        if (GetLastError() != ERROR_PIPE_BUSY) {
            print_error(L"CreateFileW");
            return 1;
        }
        if (!WaitNamedPipeW(PIPE_NAME, 5000)) {
            fwprintf(stderr, L"WaitNamedPipeW: no instance within 5 s\n");
            return 1;
        }
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL)) {
        print_error(L"SetNamedPipeHandleState");
        CloseHandle(pipe);
        return 1;
    }

    wprintf(L"client pid %lu connected\n", GetCurrentProcessId());
    for (int i = 1; i < argc; i++) {
        int len = 0;
        char *bytes = to_utf8(argv[i], &len);
        if (bytes == NULL) {
            CloseHandle(pipe);
            return 1;
        }

        DWORD written = 0;
        if (!WriteFile(pipe, bytes, (DWORD)len, &written, NULL)) {
            print_error(L"WriteFile");
            free(bytes);
            CloseHandle(pipe);
            return 1;
        }

        char reply[BUF_SIZE];
        DWORD got = 0;
        if (!ReadFile(pipe, reply, sizeof reply - 1, &got, NULL)) {
            print_error(L"ReadFile");
            free(bytes);
            CloseHandle(pipe);
            return 1;
        }
        reply[got] = '\0';

        wchar_t wide[BUF_SIZE];
        int wide_len = MultiByteToWideChar(CP_UTF8, 0, reply, (int)got, wide, BUF_SIZE - 1);
        wide[wide_len] = L'\0';
        wprintf(L"sent \"%ls\", got \"%ls\"\n", argv[i], wide);
        free(bytes);
    }

    CloseHandle(pipe);
    return 0;
}