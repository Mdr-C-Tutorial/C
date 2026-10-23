/* select_fdset.c —— Winsock 的 fd_set 是什么，select 的第一个参数为什么被忽略，
 * 以及 select 不能等待管道。gcc ... -lws2_32
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <locale.h>
#include <stdio.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 1;
    }

    /* fd_set 不是位图，而是 SOCKET 数组加计数。 */
    fd_set set;
    FD_ZERO(&set);
    printf("FD_SETSIZE = %d\n", FD_SETSIZE);
    printf("sizeof(fd_set) = %llu, sizeof(SOCKET) = %llu\n",
           (unsigned long long)sizeof(fd_set),
           (unsigned long long)sizeof(SOCKET));

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    FD_SET(s, &set);
    printf("after FD_SET: fd_count = %u, fd_array[0] = %llu\n", set.fd_count,
           (unsigned long long)set.fd_array[0]);

    /* 把监听套接字换成一对连上的套接字，演示 select 等待。 */
    SOCKET lfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = {.sin_family = AF_INET};
    addr.sin_port = htons(23470);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(lfd, (struct sockaddr *)&addr, sizeof addr);
    listen(lfd, 4);

    SOCKET c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(c, (struct sockaddr *)&addr, sizeof addr);
    SOCKET a = accept(lfd, NULL, NULL);

    /* 第一个参数传 0（Winsock 忽略它），照样能等到数据。 */
    const char *msg = "ping";
    send(c, msg, 4, 0);

    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(a, &rd);
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    int n = select(0, &rd, NULL, NULL, &tv);
    printf("select(0, ...) = %d, FD_ISSET = %d\n", n, FD_ISSET(a, &rd));
    char buf[16];
    int got = recv(a, buf, (int)sizeof buf, 0);
    printf("recv = %d: %.*s\n", got, got, buf);

    /* 超时：没有数据时返回 0，集合被清空。 */
    FD_ZERO(&rd);
    FD_SET(a, &rd);
    struct timeval tv2 = {.tv_sec = 0, .tv_usec = 300000};
    n = select(0, &rd, NULL, NULL, &tv2);
    printf("select timeout = %d, fd_count after return = %u\n", n, rd.fd_count);

    /* 管道句柄不是套接字，select 不认。 */
    HANDLE rd_pipe, wr_pipe;
    if (CreatePipe(&rd_pipe, &wr_pipe, NULL, 0)) {
        fd_set ps;
        FD_ZERO(&ps);
        FD_SET((SOCKET)(UINT_PTR)rd_pipe, &ps);
        struct timeval tv3 = {.tv_sec = 0, .tv_usec = 0};
        n = select(0, &ps, NULL, NULL, &tv3);
        printf("select on a pipe handle = %d, WSAGetLastError = %d\n", n,
               n == SOCKET_ERROR ? WSAGetLastError() : 0);
        CloseHandle(rd_pipe);
        CloseHandle(wr_pipe);
    }

    closesocket(a);
    closesocket(c);
    closesocket(lfd);
    WSACleanup();
    return 0;
}