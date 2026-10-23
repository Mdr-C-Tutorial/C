/* POSIX 里的 close(fd) 在套接字上不起作用。 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET lfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in a = {.sin_family = AF_INET};
    a.sin_port = htons(23620);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    BOOL ex = TRUE;
    setsockopt(lfd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&ex, sizeof ex);
    bind(lfd, (struct sockaddr *)&a, sizeof a);
    listen(lfd, 4);

    SOCKET c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(c, (struct sockaddr *)&a, sizeof a);
    SOCKET s = accept(lfd, NULL, NULL);
    printf("accepted socket = %llu (int 装得下吗: %s)\n",
           (unsigned long long)s, s <= INT_MAX ? "是" : "不是");

    errno = 0;
    int rc = close((int)s);
    printf("close(%d) = %d, errno = %d\n", (int)s, rc, errno);

    /* 连接没关掉：还能继续收发。 */
    const char *msg = "still alive\n";
    int n = send(s, msg, 12, 0);
    printf("send after close = %d\n", n);
    char buf[32];
    n = recv(c, buf, (int)sizeof buf, 0);
    printf("client recv = %d: %.*s", n, n > 0 ? n : 0, buf);

    rc = closesocket(s);
    printf("closesocket = %d\n", rc);
    closesocket(c);
    closesocket(lfd);
    WSACleanup();
    return 0;
}
