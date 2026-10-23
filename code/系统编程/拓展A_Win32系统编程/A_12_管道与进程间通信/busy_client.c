/* 连一次命名管道：先直接 CreateFileW，遇到 ERROR_PIPE_BUSY 再 WaitNamedPipeW 后重试。 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_busy"

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    DWORD wait_ms = argc > 1 ? (DWORD)strtoul(argv[1], NULL, 10) : 5000;
    DWORD hold_ms = argc > 2 ? (DWORD)strtoul(argv[2], NULL, 10) : 0;

    HANDLE pipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0,
                              NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        printf("CreateFileW failed: error %lu\n", err);
        if (err != ERROR_PIPE_BUSY) {
            return 1;
        }
        printf("ERROR_PIPE_BUSY, waiting up to %lu ms\n", wait_ms);
        if (!WaitNamedPipeW(PIPE_NAME, wait_ms)) {
            printf("WaitNamedPipeW failed: error %lu\n", GetLastError());
            return 1;
        }
        pipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0,
                           NULL);
        if (pipe == INVALID_HANDLE_VALUE) {
            printf("retry CreateFileW failed: error %lu\n", GetLastError());
            return 1;
        }
        printf("connected after waiting\n");
    } else {
        printf("connected immediately\n");
    }

    if (hold_ms > 0) {
        printf("holding the connection for %lu ms\n", hold_ms);
        fflush(stdout);
        Sleep(hold_ms);
    }
    CloseHandle(pipe);
    return 0;
}