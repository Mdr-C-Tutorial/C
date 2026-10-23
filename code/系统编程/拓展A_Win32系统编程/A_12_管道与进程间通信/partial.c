/* 读完半条消息之后，下一次 ReadFile 是接着读余下部分，还是读下一条消息？ */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_partial"

static DWORD WINAPI server_thread(LPVOID arg) {
    HANDLE pipe = arg;

    ConnectNamedPipe(pipe, NULL);
    const char *messages[] = {"0123456789abcdefghijklmnopqrstuvwxyz", "second-message"};
    for (int i = 0; i < 2; i++) {
        DWORD written = 0;
        WriteFile(pipe, messages[i], (DWORD)strlen(messages[i]), &written, NULL);
    }
    Sleep(300);
    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    HANDLE server_pipe = CreateNamedPipeW(PIPE_NAME, PIPE_ACCESS_DUPLEX,
                                          PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                          1, 4096, 4096, 0, NULL);
    if (server_pipe == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipeW: error %lu\n", GetLastError());
        return 1;
    }
    HANDLE thread = CreateThread(NULL, 0, server_thread, server_pipe, 0, NULL);

    HANDLE pipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0,
                              NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        printf("CreateFileW: error %lu\n", GetLastError());
        return 1;
    }
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(pipe, &mode, NULL, NULL);

    char buf[256];
    DWORD got = 0;
    BOOL ok = ReadFile(pipe, buf, 16, &got, NULL);
    printf("read with a 16-byte buffer: ok=%d, %lu bytes, error=%lu\n", ok, got,
           ok ? 0 : GetLastError());

    ok = ReadFile(pipe, buf, sizeof buf, &got, NULL);
    buf[got] = '\0';
    printf("next read with a 256-byte buffer: ok=%d, %lu bytes, error=%lu, \"%.*s\"\n", ok, got,
           ok ? 0 : GetLastError(), (int)got, buf);

    CloseHandle(pipe);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}