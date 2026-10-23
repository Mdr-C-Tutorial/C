/* 两个重叠读同时进行、都不带事件，转而等待文件句柄本身：句柄有信号只知道
   「某个操作完成了」，分不清是哪一个。 */
#include <windows.h>
#include <stdio.h>

#define BUF_SIZE 64

static DWORD WINAPI writer(void *param) {
    HANDLE h;
    DWORD n = 0;

    (void)param;
    Sleep(300);
    h = CreateFileW(L"\\\\.\\pipe\\mdr_wait_handle", GENERIC_WRITE, 0, NULL,
                    OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("writer: CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }
    Sleep(300);
    WriteFile(h, "first", 5, &n, NULL);
    printf("writer: wrote \"first\", keeps the pipe open\n");
    fflush(stdout);
    Sleep(1000);
    WriteFile(h, "second", 6, &n, NULL);
    printf("writer: wrote \"second\"\n");
    fflush(stdout);
    Sleep(300);
    CloseHandle(h);
    printf("writer: closed the pipe\n");
    fflush(stdout);
    return 0;
}

int main(void) {
    HANDLE srv;
    HANDLE th;
    OVERLAPPED ov1 = {0};
    OVERLAPPED ov2 = {0};
    static char b1[BUF_SIZE];
    static char b2[BUF_SIZE];
    DWORD n = 0;
    DWORD rc;
    BOOL ok;

    srv = CreateNamedPipeW(L"\\\\.\\pipe\\mdr_wait_handle",
                           PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                           PIPE_TYPE_BYTE | PIPE_WAIT, 1, 0, BUF_SIZE, 0, NULL);
    if (srv == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipeW failed: %lu\n", GetLastError());
        return 1;
    }
    th = CreateThread(NULL, 0, writer, NULL, 0, NULL);
    if (!ConnectNamedPipe(srv, NULL) &&
        GetLastError() != ERROR_PIPE_CONNECTED) {
        printf("ConnectNamedPipe failed: %lu\n", GetLastError());
    }
    printf("server: client connected\n");
    fflush(stdout);

    if (!ReadFile(srv, b1, BUF_SIZE, &n, &ov1) &&
        GetLastError() != ERROR_IO_PENDING) {
        printf("server: first ReadFile failed: %lu\n", GetLastError());
    }
    if (!ReadFile(srv, b2, BUF_SIZE, &n, &ov2) &&
        GetLastError() != ERROR_IO_PENDING) {
        printf("server: second ReadFile failed: %lu\n", GetLastError());
    }
    printf("server: two reads in flight, neither OVERLAPPED has an event\n");
    fflush(stdout);

    rc = WaitForSingleObject(srv, 5000);
    printf("server: waiting on the pipe handle returned %lu\n", rc);
    ok = GetOverlappedResult(srv, &ov1, &n, FALSE);
    printf("server: ov1 %s\n", ok ? "done" : "not done");
    ok = GetOverlappedResult(srv, &ov2, &n, FALSE);
    printf("server: ov2 %s (error %lu)\n", ok ? "done" : "not done",
           ok ? 0 : GetLastError());
    fflush(stdout);

    WaitForSingleObject(th, INFINITE);
    printf("server: client thread finished\n");
    ok = GetOverlappedResult(srv, &ov2, &n, TRUE);
    if (ok) {
        printf("server: ov2 completed with %lu bytes: %.*s\n", n, (int)n, b2);
    } else {
        printf("server: ov2 ended with error %lu\n", GetLastError());
    }
    CloseHandle(th);
    CloseHandle(srv);
    printf("server: done\n");
    return 0;
}