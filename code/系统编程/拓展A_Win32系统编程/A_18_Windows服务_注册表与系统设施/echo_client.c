/* 最小的回声客户端：连接 host:port，把命令行给出的每一行发出去并打印回来的内容。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o echo_client.exe echo_client.c -lws2_32
 * 用法：echo_client.exe 127.0.0.1 23456 一行文本 [更多文本...]
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    WSADATA wsa;
    SOCKADDR_IN addr;
    char buf[4096];

    if (argc < 4) {
        fprintf(stderr, "usage: %s host port text...\n", argv[0]);
        return 2;
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1) {
        fprintf(stderr, "bad address: %s\n", argv[1]);
        closesocket(s);
        WSACleanup();
        return 2;
    }

    if (connect(s, (SOCKADDR *)&addr, sizeof addr) == SOCKET_ERROR) {
        fprintf(stderr, "connect failed: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();
        return 1;
    }

    for (int i = 3; i < argc; i++) {
        int len = (int)strlen(argv[i]);
        if (send(s, argv[i], len, 0) != len) {
            fprintf(stderr, "send failed: %d\n", WSAGetLastError());
            break;
        }
        int got = 0;
        while (got < len) {
            int n = recv(s, buf + got, len - got, 0);
            if (n <= 0) {
                break;
            }
            got += n;
        }
        buf[got] = '\0';
        printf("echo: %s\n", buf);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}