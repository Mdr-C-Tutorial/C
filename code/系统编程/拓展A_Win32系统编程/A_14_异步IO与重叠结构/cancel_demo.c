/* 重叠 ConnectNamedPipe 等 300 ms 超时后用 CancelIoEx 取消，得到 ERROR_OPERATION_ABORTED。 */
#include <windows.h>
#include <stdio.h>

#define BUF_SIZE 4096

int main(void) {
    HANDLE h;
    HANDLE ev;
    OVERLAPPED ov = {0};
    DWORD n = 0;
    DWORD rc;

    ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (ev == NULL) {
        printf("CreateEventW failed: %lu\n", GetLastError());
        return 1;
    }
    ov.hEvent = ev;
    h = CreateNamedPipeW(L"\\\\.\\pipe\\mdr_cancel",
                         PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                         PIPE_TYPE_BYTE | PIPE_WAIT, 1, BUF_SIZE, BUF_SIZE, 0,
                         NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipeW failed: %lu\n", GetLastError());
        CloseHandle(ev);
        return 1;
    }
    if (ConnectNamedPipe(h, &ov)) {
        printf("ConnectNamedPipe returned TRUE\n");
    } else {
        printf("ConnectNamedPipe returned FALSE, error %lu\n", GetLastError());
    }

    rc = WaitForSingleObject(ev, 300);
    if (rc != WAIT_TIMEOUT) {
        printf("a client connected, nothing to cancel\n");
        CloseHandle(h);
        CloseHandle(ev);
        return 0;
    }
    printf("waited 300 ms: WAIT_TIMEOUT, no client\n");

    if (!CancelIoEx(h, &ov)) {
        printf("CancelIoEx failed: %lu\n", GetLastError());
    } else {
        printf("CancelIoEx succeeded\n");
    }
    if (GetOverlappedResult(h, &ov, &n, TRUE)) {
        printf("GetOverlappedResult returned TRUE, %lu bytes\n", n);
    } else {
        printf("GetOverlappedResult returned FALSE, error %lu "
               "(ERROR_OPERATION_ABORTED)\n",
               GetLastError());
    }

    CloseHandle(h);
    CloseHandle(ev);
    return 0;
}