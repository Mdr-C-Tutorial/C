/* reuse_hijack.c —— 在同一进程里演示 SO_REUSEADDR 的端口劫持与 SO_EXCLUSIVEADDRUSE 的防护。
 *
 * gcc -std=c17 -Wall -Wextra reuse_hijack.c -o reuse_hijack.exe -lws2_32
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <locale.h>
#include <stdio.h>

/* 最近一次 make_listener 失败的 WSAGetLastError()。 */
static int g_err;

/* 建一个监听套接字：wildcard 为真绑 0.0.0.0，否则绑 127.0.0.1；
 * opt 取 0 默认、1 SO_REUSEADDR、2 SO_EXCLUSIVEADDRUSE。失败返回 INVALID_SOCKET。 */
static SOCKET make_listener(unsigned short port, int wildcard, int opt) {
    g_err = 0;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        g_err = WSAGetLastError();
        return INVALID_SOCKET;
    }
    BOOL on = TRUE;
    if (opt == 1) {
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof on);
    } else if (opt == 2) {
        setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&on,
                   sizeof on);
    }
    struct sockaddr_in a = {.sin_family = AF_INET};
    a.sin_port = htons(port);
    a.sin_addr.s_addr = wildcard ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&a, sizeof a) == SOCKET_ERROR) {
        g_err = WSAGetLastError();
        closesocket(s);
        return INVALID_SOCKET;
    }
    if (listen(s, 4) == SOCKET_ERROR) {
        g_err = WSAGetLastError();
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

/* 连一次自己的端口，返回哪个监听套接字接到了这个连接（0 或 1），都没有返回 -1。 */
static int who_answers(SOCKET listeners[2], unsigned short port) {
    SOCKET c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in a = {.sin_family = AF_INET};
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(c, (struct sockaddr *)&a, sizeof a) == SOCKET_ERROR) {
        printf("  connect failed: %d\n", WSAGetLastError());
        closesocket(c);
        return -1;
    }
    int who = -1;
    for (int i = 0; i < 2; i++) {
        if (listeners[i] == INVALID_SOCKET) {
            continue;
        }
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(listeners[i], &rd);
        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        if (select(0, &rd, NULL, NULL, &tv) > 0) {
            SOCKET s = accept(listeners[i], NULL, NULL);
            closesocket(s);
            who = i;
            break;
        }
    }
    closesocket(c);
    return who;
}

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 1;
    }

    /* 场景一：服务只设默认选项，绑在通配地址上；后来者用 SO_REUSEADDR 绑 127.0.0.1。 */
    unsigned short port = 23610;
    SOCKET listeners[2];
    listeners[0] = make_listener(port, 1, 0);
    listeners[1] = make_listener(port, 0, 1);
    printf("service: default + 0.0.0.0 -> %s\n",
           listeners[0] == INVALID_SOCKET ? "bind failed" : "bind ok");
    printf("attacker: SO_REUSEADDR + 127.0.0.1 -> %s (error %d)\n",
           listeners[1] == INVALID_SOCKET ? "bind failed" : "bind ok",
           g_err);
    if (listeners[0] != INVALID_SOCKET && listeners[1] != INVALID_SOCKET) {
        int who = who_answers(listeners, port);
        printf("connection answered by: %s\n",
               who == 0 ? "service" : who == 1 ? "attacker" : "nobody");
    }
    for (int i = 0; i < 2; i++) {
        if (listeners[i] != INVALID_SOCKET) {
            closesocket(listeners[i]);
        }
    }

    /* 场景二：服务设 SO_EXCLUSIVEADDRUSE，同样的入侵者。 */
    port = 23611;
    listeners[0] = make_listener(port, 1, 2);
    listeners[1] = make_listener(port, 0, 1);
    printf("service: SO_EXCLUSIVEADDRUSE + 0.0.0.0 -> %s\n",
           listeners[0] == INVALID_SOCKET ? "bind failed" : "bind ok");
    printf("attacker: SO_REUSEADDR + 127.0.0.1 -> %s (error %d)\n",
           listeners[1] == INVALID_SOCKET ? "bind failed" : "bind ok",
           g_err);
    for (int i = 0; i < 2; i++) {
        if (listeners[i] != INVALID_SOCKET) {
            closesocket(listeners[i]);
        }
    }

    WSACleanup();
    return 0;
}