/* CallNamedPipeW：一次调用完成连接、发送、接收、断开。 */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_transact"
#define BUF_SIZE 4096

static DWORD WINAPI server_thread(LPVOID arg) {
    HANDLE pipe = arg;

    if (!ConnectNamedPipe(pipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
        wprintf(L"ConnectNamedPipe: error %lu\n", GetLastError());
        return 1;
    }
    char request[BUF_SIZE];
    DWORD got = 0;
    ReadFile(pipe, request, sizeof request, &got, NULL);
    request[got] = '\0';

    char reply[BUF_SIZE];
    int n = snprintf(reply, sizeof reply, "server echoes: %s", request);
    DWORD written = 0;
    WriteFile(pipe, reply, (DWORD)n, &written, NULL);
    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    HANDLE pipe = CreateNamedPipeW(PIPE_NAME, PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
                                   4096, 4096, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        wprintf(L"CreateNamedPipeW: error %lu\n", GetLastError());
        return 1;
    }
    HANDLE thread = CreateThread(NULL, 0, server_thread, pipe, 0, NULL);

    char reply[BUF_SIZE];
    DWORD got = 0;
    BOOL ok = CallNamedPipeW(PIPE_NAME, "ping", 4, reply, sizeof reply, &got, 2000);
    if (!ok) {
        wprintf(L"CallNamedPipeW: error %lu\n", GetLastError());
    } else {
        reply[got] = '\0';
        wprintf(L"one call, %lu bytes back: \"%hs\"\n", got, reply);
    }

    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return ok ? 0 : 1;
}