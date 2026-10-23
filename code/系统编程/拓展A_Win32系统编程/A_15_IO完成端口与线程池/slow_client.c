/* 测试用慢速客户端：连接 127.0.0.1:23456，每 300 ms 发一行，收到回显再发下一行。 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    WSADATA wsa;
    const char *tag = argc > 1 ? argv[1] : "client";
    const char *port = argc > 2 ? argv[2] : "23456";

    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)atoi(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, (struct sockaddr *)&addr, sizeof addr) != 0) {
        printf("%s: connect 失败 %d\n", tag, WSAGetLastError());
        return 1;
    }

    char line[128];
    char buf[512];
    for (int i = 1; i <= 3; i++) {
        int len = snprintf(line, sizeof line, "%s line %d\n", tag, i);
        send(s, line, len, 0);
        int n = recv(s, buf, (int)sizeof buf, 0);
        if (n > 0) {
            fwrite(buf, 1, (size_t)n, stdout);
        }
        Sleep(300);
    }
    closesocket(s);
    WSACleanup();
    return 0;
}