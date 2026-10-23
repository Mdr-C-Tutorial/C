/* 重叠句柄上的读也可能同步完成：ReadFile 返回 TRUE。 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 256

int main(void) {
    HANDLE srv;
    HANDLE cli;
    OVERLAPPED conn = {0};
    OVERLAPPED ov = {0};
    char buf[BUF_SIZE];
    DWORD n = 0;
    DWORD written = 0;
    const char *msg = "already here";
    BOOL ok;

    srv = CreateNamedPipeW(
        L"\\\\.\\pipe\\mdr_sync",
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT, 1, BUF_SIZE, BUF_SIZE, 0, NULL);
    if (srv == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipeW failed: %lu\n", GetLastError());
        return 1;
    }
    conn.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!ConnectNamedPipe(srv, &conn)) {
        DWORD err = GetLastError();

        if (err != ERROR_PIPE_CONNECTED && err != ERROR_IO_PENDING) {
            printf("ConnectNamedPipe failed: %lu\n", err);
            return 1;
        }
    }
    cli = CreateFileW(L"\\\\.\\pipe\\mdr_sync", GENERIC_WRITE, 0, NULL,
                      OPEN_EXISTING, 0, NULL);
    if (cli == INVALID_HANDLE_VALUE) {
        printf("client CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(conn.hEvent, INFINITE);
    printf("client connected\n");

    WriteFile(cli, msg, (DWORD)strlen(msg), &written, NULL);
    printf("client wrote %lu bytes, the server has not asked for them yet\n",
           written);

    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ok = ReadFile(srv, buf, BUF_SIZE, &n, &ov);
    printf("ReadFile returned %s, n = %lu, data = %.*s\n",
           ok ? "TRUE" : "FALSE", n, (int)n, buf);
    if (!ok) {
        printf("(it came back FALSE, error %lu; this run did not complete "
               "synchronously)\n",
               GetLastError());
    }

    CloseHandle(cli);
    CloseHandle(srv);
    return 0;
}