/* echo_portable.c —— 同一份源码在 Windows 与 Linux 上编译的 TCP 回声服务器。
 *
 * Windows: gcc -std=c17 -Wall -Wextra echo_portable.c -o echo_server.exe -lws2_32
 * Linux:   cc -std=c17 -Wall -Wextra echo_portable.c -o echo_server
 *
 * 用法: echo_server [端口]，默认 23456，只监听 127.0.0.1。
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET sock_t;
#define sock_close(s) closesocket(s)
#define sock_invalid INVALID_SOCKET
#define SOCK_ERR SOCKET_ERROR
#else
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define sock_close(s) close(s)
#define sock_invalid (-1)
#define SOCK_ERR (-1)
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 取最近一次套接字调用的错误码，两边名字不同。 */
static int sock_errno(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

/* 判断「暂时没有数据、稍后再试」的错误码。 */
static int sock_would_block(int e) {
#ifdef _WIN32
    return e == WSAEWOULDBLOCK;
#else
    return e == EAGAIN || e == EWOULDBLOCK || e == EINTR;
#endif
}

static void die(const char *what) {
    fprintf(stderr, "%s: error %d\n", what, sock_errno());
    exit(EXIT_FAILURE);
}

/* 监听 127.0.0.1 上指定端口，返回监听套接字。 */
static sock_t tcp_listen(const char *port) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo("127.0.0.1", port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    sock_t fd = sock_invalid;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == sock_invalid) {
            continue;
        }
#ifdef _WIN32
        /* Windows 服务器独占端口，防止别的进程用 SO_REUSEADDR 抢绑。 */
        BOOL exclusive = TRUE;
        if (setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       (const char *)&exclusive, sizeof exclusive) ==
            SOCK_ERR) {
            sock_close(fd);
            fd = sock_invalid;
            continue;
        }
#else
        /* POSIX 上让重启时能立刻重新绑定处于 TIME_WAIT 的端口。 */
        int one = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) ==
            SOCK_ERR) {
            sock_close(fd);
            fd = sock_invalid;
            continue;
        }
#endif
        if (bind(fd, p->ai_addr, (int)p->ai_addrlen) == 0) {
            break;
        }
        sock_close(fd);
        fd = sock_invalid;
    }
    freeaddrinfo(res);

    if (fd == sock_invalid) {
        die("bind");
    }
    if (listen(fd, 16) == SOCK_ERR) {
        die("listen");
    }
    return fd;
}

/* 把收到的字节原样发回，直到对端关闭。 */
static void serve(sock_t c) {
    char buf[4096];
    for (;;) {
        int n = recv(c, buf, (int)sizeof buf, 0);
        if (n == 0) {
            return;
        }
        if (n < 0) {
            if (sock_would_block(sock_errno())) {
                continue;
            }
            return;
        }
        int sent = 0;
        while (sent < n) {
            int m = send(c, buf + sent, n - sent, 0);
            if (m == SOCK_ERR) {
                if (sock_would_block(sock_errno())) {
                    continue;
                }
                return;
            }
            sent += m;
        }
    }
}

/* 先关发送方向，把对端剩下的数据读完，再关闭套接字。 */
static void close_gracefully(sock_t c) {
#ifdef _WIN32
    shutdown(c, SD_SEND);
#else
    shutdown(c, SHUT_WR);
#endif
    char buf[512];
    while (recv(c, buf, (int)sizeof buf, 0) > 0) {
    }
    sock_close(c);
}

int main(int argc, char **argv) {
    const char *port = argc > 1 ? argv[1] : "23456";

#ifdef _WIN32
    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", rc);
        return EXIT_FAILURE;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    sock_t lfd = tcp_listen(port);
    printf("listening on 127.0.0.1:%s\n", port);
    fflush(stdout);

    for (;;) {
        struct sockaddr_storage peer;
        socklen_t peer_len = sizeof peer;
        sock_t c = accept(lfd, (struct sockaddr *)&peer, &peer_len);
        if (c == sock_invalid) {
            if (sock_would_block(sock_errno())) {
                continue;
            }
            die("accept");
        }
        printf("connection accepted\n");
        fflush(stdout);
        serve(c);
        close_gracefully(c);
        printf("connection closed\n");
        fflush(stdout);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}