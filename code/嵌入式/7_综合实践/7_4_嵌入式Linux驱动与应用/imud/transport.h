#define _POSIX_C_SOURCE 200809L

#ifndef IMUD_TRANSPORT_H
#define IMUD_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#define TRANSPORT_RECV_TIMEOUT_MS 10

/*
 * coreMQTT 要求应用实现 struct NetworkContext 与 send/recv 两个函数。
 * 这里把它定义成一个 TCP 描述符加一个接收超时，与 7.3 的 ESP-AT、
 * Winsock 两个实现保持同一套接口；换传输层时 mqtt_glue.c 不动。
 */
struct NetworkContext {
    int fd;
    int recv_timeout_ms;
};

/* 连接 127.0.0.1:port 或 host:port，成功返回 0。 */
int transport_connect(struct NetworkContext *ctx, const char *host,
                      const char *port);

void transport_close(struct NetworkContext *ctx);

int32_t transport_send(struct NetworkContext *ctx, const void *buf,
                       size_t len);

int32_t transport_recv(struct NetworkContext *ctx, void *buf, size_t len);

#endif