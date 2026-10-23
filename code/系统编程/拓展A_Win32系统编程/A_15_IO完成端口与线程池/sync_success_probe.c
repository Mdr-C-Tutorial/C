/* 验证：重叠套接字关联到完成端口后，WSARecv 若立即成功返回，
 * 完成包是否仍会排队。 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdio.h>

static SOCKET g_client;

static DWORD WINAPI sender(LPVOID arg) {
    (void)arg;
    Sleep(200);
    send(g_client, "hello iocp", 10, 0);
    return 0;
}

int main(void) {
    WSADATA wsa;
    struct sockaddr_in addr = {0};
    char buf[64];
    WSABUF wsabuf;
    OVERLAPPED ov = {0};
    DWORD n = 0;
    DWORD flags = 0;
    SOCKET listen_sock;
    SOCKET server;
    HANDLE port;
    HANDLE th;
    DWORD count = 0;
    ULONG_PTR key = 0;
    OVERLAPPED *pov = NULL;
    BOOL got;
    int rc;

    WSAStartup(MAKEWORD(2, 2), &wsa);

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(23458);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listen_sock, (struct sockaddr *)&addr, sizeof addr);
    listen(listen_sock, 1);

    g_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(g_client, (struct sockaddr *)&addr, sizeof addr);
    server = accept(listen_sock, NULL, NULL);

    port = CreateIoCompletionPort((HANDLE)server, NULL, 42, 0);

    th = CreateThread(NULL, 0, sender, NULL, 0, NULL);

    /* 等数据真正进入接收缓冲区，这样 WSARecv 会立即完成 */
    fd_set set;
    FD_ZERO(&set);
    FD_SET(server, &set);
    select(0, &set, NULL, NULL, NULL);
    printf("数据已到达，开始 WSARecv\n");

    wsabuf.buf = buf;
    wsabuf.len = sizeof buf;
    rc = WSARecv(server, &wsabuf, 1, &n, &flags, &ov, NULL);
    printf("WSARecv 返回 %d，错误 %lu，立即读到 %lu 字节\n", rc, GetLastError(),
           (unsigned long)n);

    got = GetQueuedCompletionStatus(port, &count, &key, &pov, 1000);
    printf("取完成包: 返回 %d，字节 %lu，完成键 %llu，指针%s\n", got,
           (unsigned long)count, (unsigned long long)key,
           pov == &ov ? "就是刚才那个" : "不同");

    WaitForSingleObject(th, 2000);
    CloseHandle(th);
    closesocket(listen_sock);
    closesocket(server);
    closesocket(g_client);
    CloseHandle(port);
    WSACleanup();
    return 0;
}