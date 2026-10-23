/* 消息模式：一次 WriteFile 是一条消息；缓冲区不够时 GetLastError 为 ERROR_MORE_DATA。 */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_msgmode"
#define SMALL_BUF 16

static DWORD WINAPI server_thread(LPVOID arg) {
    HANDLE pipe = arg;

    if (!ConnectNamedPipe(pipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
        wprintf(L"ConnectNamedPipe: error %lu\n", GetLastError());
        CloseHandle(pipe);
        return 1;
    }

    /* 三条消息，第三条比读缓冲区长。 */
    const char *messages[] = {"alpha", "bravo", "0123456789abcdefghijklmnopqrstuvwxyz"};
    for (int i = 0; i < 3; i++) {
        DWORD written = 0;
        DWORD len = (DWORD)strlen(messages[i]);
        if (!WriteFile(pipe, messages[i], len, &written, NULL)) {
            wprintf(L"WriteFile: error %lu\n", GetLastError());
        }
        wprintf(L"server: wrote message %d, %lu bytes\n", i + 1, written);
    }

    Sleep(500); /* 等客户端读完再把连接断掉 */
    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    /* 先建好实例，再让客户端来连，避免客户端抢在 CreateNamedPipeW 前面。 */
    HANDLE server_pipe = CreateNamedPipeW(
        PIPE_NAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, 4096, 4096, 0, NULL);
    if (server_pipe == INVALID_HANDLE_VALUE) {
        wprintf(L"CreateNamedPipeW: error %lu\n", GetLastError());
        return 1;
    }

    HANDLE thread = CreateThread(NULL, 0, server_thread, server_pipe, 0, NULL);
    if (thread == NULL) {
        wprintf(L"CreateThread: error %lu\n", GetLastError());
        CloseHandle(server_pipe);
        return 1;
    }

    HANDLE pipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                              0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        wprintf(L"CreateFileW: error %lu\n", GetLastError());
        return 1;
    }
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(pipe, &mode, NULL, NULL);

    /* 第一部分：用足够大的缓冲区读，一条消息一次读完，边界保住了。 */
    char big[256];
    for (int i = 0; i < 2; i++) {
        DWORD got = 0;
        BOOL ok = ReadFile(pipe, big, sizeof big, &got, NULL);
        big[got] = '\0';
        wprintf(L"read with 256-byte buffer: ok=%d, %lu bytes, \"%hs\"\n", ok, got, big);
    }

    /* 第二部分：用 16 字节的缓冲区读同一条消息。 */
    char small[SMALL_BUF];
    for (;;) {
        DWORD got = 0;
        BOOL ok = ReadFile(pipe, small, sizeof small, &got, NULL);
        DWORD err = ok ? 0 : GetLastError();
        wprintf(L"read with %d-byte buffer: ok=%d, %lu bytes, error=%lu\n", SMALL_BUF, ok,
                got, err);
        if (ok || err != ERROR_MORE_DATA) {
            break;
        }
    }

    CloseHandle(pipe);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}