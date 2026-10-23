/* shutdown(SD_SEND) 之后还能不能 recv、还能不能 send；FIONREAD 读可读字节数。 */
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
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET lfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in a = {.sin_family = AF_INET};
    a.sin_port = htons(23560);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    BOOL ex = TRUE;
    setsockopt(lfd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&ex, sizeof ex);
    bind(lfd, (struct sockaddr *)&a, sizeof a);
    listen(lfd, 4);

    SOCKET c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(c, (struct sockaddr *)&a, sizeof a);
    SOCKET s = accept(lfd, NULL, NULL);

    send(c, "0123456789", 10, 0);
    Sleep(100);
    u_long avail = 0;
    ioctlsocket(s, FIONREAD, &avail);
    printf("FIONREAD before recv: %lu\n", avail);

    int rc = shutdown(c, SD_SEND);
    printf("shutdown(SD_SEND) = %d\n", rc);
    char byte = 'x';
    rc = send(c, &byte, 1, 0);
    printf("send after shutdown(SD_SEND) = %d, error %d\n", rc,
           rc == SOCKET_ERROR ? WSAGetLastError() : 0);

    char buf[32];
    int n = recv(s, buf, (int)sizeof buf, 0);
    printf("server recv %d bytes: %.*s\n", n, n > 0 ? n : 0, buf);
    n = recv(s, buf, (int)sizeof buf, 0);
    printf("server recv again = %d (0 = EOF)\n", n);

    /* 服务器也半关闭，客户端接着读。 */
    send(s, "reply", 5, 0);
    shutdown(s, SD_SEND);
    n = recv(c, buf, (int)sizeof buf, 0);
    printf("client recv after reply: %d bytes: %.*s\n", n, n > 0 ? n : 0, buf);
    n = recv(c, buf, (int)sizeof buf, 0);
    printf("client recv again = %d (0 = EOF)\n", n);

    closesocket(c);
    closesocket(s);
    closesocket(lfd);
    WSACleanup();
    return 0;
}
