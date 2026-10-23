/* echo_client.c —— 与 echo_portable.c 配套的客户端，两平台通用。
 *
 * Windows: gcc -std=c17 -Wall -Wextra echo_client.c -o echo_client.exe -lws2_32
 * Linux:   cc -std=c17 -Wall -Wextra echo_client.c -o echo_client
 *
 * 用法: echo_client [主机] [端口] [消息...]，默认 127.0.0.1 23456。
 * 每条消息各自组成一行发送，服务器回什么就打印什么。
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

static int sock_errno(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static sock_t tcp_connect(const char *host, const char *port) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, port, &hints, &res);
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
        if (connect(fd, p->ai_addr, (int)p->ai_addrlen) == 0) {
            break;
        }
        sock_close(fd);
        fd = sock_invalid;
    }
    freeaddrinfo(res);

    if (fd == sock_invalid) {
        fprintf(stderr, "connect: error %d\n", sock_errno());
        exit(EXIT_FAILURE);
    }
    return fd;
}

/* 发送全部字节，返回实际发送数（出错时小于 len）。 */
static int send_all(sock_t fd, const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(fd, buf + sent, len - sent, 0);
        if (n == SOCK_ERR) {
            return sent;
        }
        sent += n;
    }
    return sent;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return EXIT_FAILURE;
    }
#endif

    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    const char *port = argc > 2 ? argv[2] : "23456";
    const char *default_msgs[] = {"hello winsock", "second line"};
    int first = 3;
    int count = argc > first ? argc - first : 2;

    sock_t fd = tcp_connect(host, port);

    char line[512];
    for (int i = 0; i < count; i++) {
        const char *msg = argc > first ? argv[first + i] : default_msgs[i];
        int len = snprintf(line, sizeof line, "%s\n", msg);
        send_all(fd, line, len);
    }

    /* 关掉发送方向，服务器读到 EOF 后同样半关闭，recv 返回 0 结束。 */
#ifdef _WIN32
    shutdown(fd, SD_SEND);
#else
    shutdown(fd, SHUT_WR);
#endif

    char buf[512];
    for (;;) {
        int n = recv(fd, buf, (int)sizeof buf, 0);
        if (n <= 0) {
            break;
        }
        fwrite(buf, 1, (size_t)n, stdout);
    }
    sock_close(fd);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}