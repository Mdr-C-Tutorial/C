/* 写错了会怎样：OVERLAPPED 和缓冲区是局部对象，发起重叠读之后函数立刻返回。
   读完成时系统把数据写进已经失效的栈帧。 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE g_pipe;
static OVERLAPPED g_ov; /* 这里只演示缓冲区的生存期问题，OVERLAPPED 放在外面 */

/* 错误写法：buf 是这个函数的局部对象。 */
static void issue_read_bad(void) {
    char buf[256];
    DWORD n = 0;

    if (!ReadFile(g_pipe, buf, sizeof buf, &n, &g_ov)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            printf("ReadFile failed: %lu\n", GetLastError());
            return;
        }
    }
    printf("issue_read_bad: read issued into buf at %p, function returns\n",
           (void *)buf);
}

/* 之后调用，栈帧正好覆盖 issue_read_bad 的那一块。它保持存活，等读完成。 */
static void live_frame(HANDLE done) {
    char buf[256];
    int i;

    memset(buf, 'L', sizeof buf);
    printf("live_frame: buf at %p, filled with 'L'\n", (void *)buf);
    WaitForSingleObject(done, 5000);
    printf("live_frame: after the read completed, buf is: ");
    for (i = 0; i < 24; i++) {
        printf("%c", buf[i]);
    }
    printf("\n");
}

static DWORD WINAPI client_thread(void *param) {
    HANDLE done = param;
    HANDLE h;
    DWORD n = 0;
    const char *msg = "client says hello";

    Sleep(200);
    h = CreateFileW(L"\\\\.\\pipe\\mdr_stale", GENERIC_WRITE, 0, NULL,
                    OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("client: CreateFileW failed: %lu\n", GetLastError());
        SetEvent(done);
        return 1;
    }
    Sleep(300);
    WriteFile(h, msg, (DWORD)strlen(msg), &n, NULL);
    printf("client: wrote %lu bytes\n", n);
    CloseHandle(h);
    SetEvent(done);
    return 0;
}

int main(void) {
    HANDLE done;
    HANDLE th;

    g_pipe = CreateNamedPipeW(
        L"\\\\.\\pipe\\mdr_stale",
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT, 1, 0, 256, 0, NULL);
    if (g_pipe == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipeW failed: %lu\n", GetLastError());
        return 1;
    }
    g_ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    done = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_ov.hEvent == NULL || done == NULL) {
        printf("CreateEventW failed: %lu\n", GetLastError());
        return 1;
    }
    th = CreateThread(NULL, 0, client_thread, done, 0, NULL);
    if (th == NULL) {
        printf("CreateThread failed: %lu\n", GetLastError());
        return 1;
    }
    printf("main: waiting for the client to connect\n");
    if (!ConnectNamedPipe(g_pipe, NULL) &&
        GetLastError() != ERROR_PIPE_CONNECTED) {
        printf("ConnectNamedPipe failed: %lu\n", GetLastError());
        return 1;
    }
    printf("main: client connected\n");
    issue_read_bad();
    live_frame(done);
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);
    CloseHandle(done);
    CloseHandle(g_ov.hEvent);
    CloseHandle(g_pipe);
    return 0;
}