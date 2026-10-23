#define _POSIX_C_SOURCE 200809L

#include "transport.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int transport_connect(struct NetworkContext *ctx, const char *host,
                      const char *port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int fd = -1;
    int ret;
    struct timeval tv = {.tv_sec = 0,
                         .tv_usec = TRANSPORT_RECV_TIMEOUT_MS * 1000};

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    ret = getaddrinfo(host, port, &hints, &res);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo %s:%s: %s\n", host, port,
                gai_strerror(ret));
        return -1;
    }
    fd = socket(res->ai_family, res->ai_socktype | SOCK_CLOEXEC,
                res->ai_protocol);
    if (fd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        fprintf(stderr, "连接 %s:%s 失败: %s\n", host, port, strerror(errno));
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    {
        int one = 1;

        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    }

    /*
     * 接收超时必须短，短到 coreMQTT 的轮询节奏不被拖慢：
     * 库认为 recv 返回 0 表示"暂时没数据、可以再调"，正是靠这一点
     * MQTT_ProcessLoop 才能在事件循环的定时器里非阻塞地跑一圈。
     * SO_RCVTIMEO 的 10 ms 让 recv 最多等这么久，超时后返回 EAGAIN，
     * 下面把它翻译成 0。
     */
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    ctx->fd = fd;
    ctx->recv_timeout_ms = TRANSPORT_RECV_TIMEOUT_MS;
    return 0;
}

void transport_close(struct NetworkContext *ctx) {
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
}

int32_t transport_send(struct NetworkContext *ctx, const void *buf,
                       size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(ctx->fd, (const unsigned char *)buf + sent,
                         len - sent, MSG_NOSIGNAL);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }
    return (int32_t)sent;
}

int32_t transport_recv(struct NetworkContext *ctx, void *buf, size_t len) {
    (void)ctx;

    for (;;) {
        ssize_t n = recv(ctx->fd, buf, len, 0);

        if (n > 0) {
            return (int32_t)n;
        }
        if (n == 0) {
            /* 对端关闭：coreMQTT 要求这时返回负值表示连接断开 */
            return -1;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; /* 没有数据，按接口约定返回 0 */
        }
        return -1;
    }
}