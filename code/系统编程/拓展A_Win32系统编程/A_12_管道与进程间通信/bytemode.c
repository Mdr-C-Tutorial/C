/* 同一个消息管道，客户端不切换到消息读模式会怎样。 */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_bytemode"

static DWORD WINAPI server_thread(LPVOID arg) {
    HANDLE pipe = arg;

    if (!ConnectNamedPipe(pipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
        wprintf(L"ConnectNamedPipe: error %lu\n", GetLastError());
        return 1;
    }
    const char *messages[] = {"one", "two", "three"};
    for (int i = 0; i < 3; i++) {
        DWORD written = 0;
        WriteFile(pipe, messages[i], (DWORD)strlen(messages[i]), &written, NULL);
    }
    Sleep(300);
    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    int message_mode = argc > 1 && argv[1][0] == 'm';

    HANDLE server_pipe = CreateNamedPipeW(
        PIPE_NAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
        4096, 4096, 0, NULL);
    if (server_pipe == INVALID_HANDLE_VALUE) {
        wprintf(L"CreateNamedPipeW: error %lu\n", GetLastError());
        return 1;
    }
    HANDLE thread = CreateThread(NULL, 0, server_thread, server_pipe, 0, NULL);

    HANDLE pipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0,
                              NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        wprintf(L"CreateFileW: error %lu\n", GetLastError());
        return 1;
    }
    if (message_mode) {
        DWORD mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
    }
    wprintf(L"client reads in %ls mode\n", message_mode ? L"message" : L"byte");

    char buf[64];
    for (int i = 0; i < 3; i++) {
        DWORD got = 0;
        BOOL ok = ReadFile(pipe, buf, sizeof buf, &got, NULL);
        wprintf(L"  read %d: ok=%d, %lu bytes, \"%.*hs\"\n", i + 1, ok, got, (int)got, buf);
    }

    CloseHandle(pipe);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}