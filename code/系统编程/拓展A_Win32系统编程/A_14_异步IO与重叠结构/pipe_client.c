/* 命令行客户端：连上 \\.\pipe\mdr_echo，每条消息发出去再读回。 */
#include <windows.h>
#include <stdio.h>

#define BUF_SIZE 4096

int wmain(int argc, wchar_t **argv) {
    HANDLE h;
    char msg[BUF_SIZE];
    char reply[BUF_SIZE];
    DWORD n = 0;
    DWORD written = 0;
    int delay_ms = 0;
    int first = 1;
    int i;

    if (argc > 2 && wcscmp(argv[1], L"-d") == 0) {
        delay_ms = _wtoi(argv[2]);
        if (delay_ms < 0) {
            delay_ms = 0;
        }
        first = 3;
    }
    if (first >= argc) {
        printf("usage: pipe_client [-d delay_ms] <message> [message ...]\n");
        return 1;
    }
    h = CreateFileW(L"\\\\.\\pipe\\mdr_echo", GENERIC_READ | GENERIC_WRITE, 0,
                    NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    {
        DWORD mode = PIPE_READMODE_MESSAGE;

        if (!SetNamedPipeHandleState(h, &mode, NULL, NULL)) {
            printf("SetNamedPipeHandleState failed: %lu\n", GetLastError());
            CloseHandle(h);
            return 1;
        }
    }
    for (i = first; i < argc; i++) {
        int len = (int)wcslen(argv[i]);
        int k;

        if (len > BUF_SIZE - 1) {
            len = BUF_SIZE - 1;
        }
        for (k = 0; k < len; k++) {
            msg[k] = (char)argv[i][k];
        }
        if (!WriteFile(h, msg, (DWORD)len, &written, NULL)) {
            printf("WriteFile failed: %lu\n", GetLastError());
            break;
        }
        if (!ReadFile(h, reply, BUF_SIZE, &n, NULL)) {
            printf("ReadFile failed: %lu\n", GetLastError());
            break;
        }
        printf("sent \"%.*s\", got \"%.*s\"\n", (int)written, msg, (int)n,
               reply);
        if (delay_ms > 0 && i + 1 < argc) {
            Sleep(delay_ms);
        }
    }
    CloseHandle(h);
    return 0;
}