/* 测试用空闲客户端：连上 127.0.0.1:23456 后什么都不发，睡若干秒再关。 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    WSADATA wsa;
    int seconds = argc > 1 ? atoi(argv[1]) : 5;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(23456);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, (struct sockaddr *)&addr, sizeof addr) != 0) {
        printf("connect 失败 %d\n", WSAGetLastError());
        return 1;
    }
    printf("已连接，保持 %d 秒\n", seconds);
    Sleep((DWORD)seconds * 1000);
    closesocket(s);
    WSACleanup();
    return 0;
}