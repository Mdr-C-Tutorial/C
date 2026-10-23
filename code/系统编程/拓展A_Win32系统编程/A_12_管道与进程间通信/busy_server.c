/* 单实例管道服务器：一次只接受一个连接，用来观察 ERROR_PIPE_BUSY 与 WaitNamedPipe。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

#define PIPE_NAME L"\\\\.\\pipe\\mdr_busy"

static void show_error(const char *what, DWORD err) {
    WCHAR *msg = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&msg, 0, NULL);
    printf("%s: error %lu: %ls", what, err, msg != NULL ? msg : L"");
    LocalFree(msg);
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    HANDLE pipe = CreateNamedPipeW(PIPE_NAME, PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
                                   4096, 4096, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        show_error("CreateNamedPipeW", GetLastError());
        return 1;
    }
    printf("server ready, one instance only\n");
    fflush(stdout);

    if (!ConnectNamedPipe(pipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
        show_error("ConnectNamedPipe", GetLastError());
        return 1;
    }
    printf("client 1 connected, holding the instance for 2 s\n");
    fflush(stdout);
    Sleep(2000);

    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);

    /* 实例断开后重新进入监听，等在 WaitNamedPipe 里的客户端就能连上。 */
    if (!ConnectNamedPipe(pipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
        show_error("ConnectNamedPipe", GetLastError());
        return 1;
    }
    printf("client 2 connected after the instance became free\n");
    fflush(stdout);
    Sleep(300);

    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    printf("server done\n");
    return 0;
}