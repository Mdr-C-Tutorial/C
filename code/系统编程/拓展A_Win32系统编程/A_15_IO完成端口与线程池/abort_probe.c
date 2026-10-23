/* 对比三种结束挂起 I/O 的方式在完成端口上产生什么结果：
 * 另一线程 closesocket、CancelIoEx、shutdown。
 * 三种情况都先对一个没有数据的套接字发起 WSARecv。 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdio.h>

#define PORT 23457

static SOCKET g_server;

static DWORD WINAPI aborter(LPVOID arg) {
    int mode = (int)(INT_PTR)arg;

    Sleep(300);
    if (mode == 0) {
        closesocket(g_server);
    } else if (mode == 1) {
        CancelIoEx((HANDLE)g_server, NULL);
    } else {
        shutdown(g_server, SD_BOTH);
    }
    return 0;
}

static void run(int mode) {
    WSADATA wsa;
    struct sockaddr_in addr = {0};
    SOCKET listen_sock;
    SOCKET client;
    char buf[64];
    WSABUF wsabuf;
    OVERLAPPED ov = {0};
    DWORD n = 0;
    DWORD flags = 0;
    ULONG_PTR key = 0;
    OVERLAPPED *pov = NULL;
    HANDLE th;
    BOOL got;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listen_sock, (struct sockaddr *)&addr, sizeof addr);
    listen(listen_sock, 1);

    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(client, (struct sockaddr *)&addr, sizeof addr);
    g_server = accept(listen_sock, NULL, NULL);
    CreateIoCompletionPort((HANDLE)g_server, port, 0, 0);

    th = CreateThread(NULL, 0, aborter, (LPVOID)(INT_PTR)mode, 0, NULL);

    wsabuf.buf = buf;
    wsabuf.len = sizeof buf;
    if (WSARecv(g_server, &wsabuf, 1, &n, &flags, &ov, NULL) != SOCKET_ERROR) {
        printf("  这一轮 WSARecv 立即完成了\n");
    }

    got = GetQueuedCompletionStatus(port, &n, &key, &pov, 2000);
    if (got) {
        printf("  完成包正常返回\n");
    } else if (pov == NULL) {
        printf("  没有完成包，调用超时（%lu）\n", GetLastError());
    } else {
        DWORD err = GetLastError();

        printf("  完成包错误 %lu", err);
        if (err == ERROR_OPERATION_ABORTED) {
            printf("（ERROR_OPERATION_ABORTED）");
        } else if (err == ERROR_CONNECTION_ABORTED) {
            printf("（ERROR_CONNECTION_ABORTED）");
        }
        printf("\n");
    }

    WaitForSingleObject(th, 3000);
    CloseHandle(th);
    closesocket(client);
    closesocket(g_server);
    closesocket(listen_sock);
    CloseHandle(port);
    WSACleanup();
}

int main(void) {
    printf("mode 0：另一线程 closesocket\n");
    run(0);
    printf("mode 1：另一线程 CancelIoEx\n");
    run(1);
    printf("mode 2：另一线程 shutdown(SD_BOTH)\n");
    run(2);
    return 0;
}