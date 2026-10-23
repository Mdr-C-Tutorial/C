#define _POSIX_C_SOURCE 200809L

/*
 * fake_broker —— 只够联调用的 MQTT 3.1.1 服务器。
 *
 * 处理 CONNECT/CONNACK、SUBSCRIBE/SUBACK、QoS 0 PUBLISH、PINGREQ/PINGRESP、
 * DISCONNECT。收到 PUBLISH 就把主题与负载打印到标准输出，不保存、不转发。
 * 7.3 的同类程序跑在 Winsock 上，这份改成 POSIX 套接字，端口只绑 127.0.0.1。
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_PACKET 4096
#define DEFAULT_PORT 1883

static volatile sig_atomic_t g_stop;

static void on_term(int sig) {
    (void)sig;
    g_stop = 1;
}

static int read_full(int fd, unsigned char *buf, size_t n) {
    size_t got = 0;

    while (got < n) {
        ssize_t r = recv(fd, buf + got, n - got, 0);

        if (r <= 0) {
            return -1;
        }
        got += (size_t)r;
    }
    return 0;
}

/* 读固定头之后的剩余长度：每字节 7 位，最高位为续读标志 */
static int read_remaining_length(int fd, size_t *out) {
    size_t value = 0;
    unsigned shift = 0;

    for (;;) {
        unsigned char byte;

        if (read_full(fd, &byte, 1) < 0) {
            return -1;
        }
        value |= (size_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            break;
        }
        shift += 7;
        if (shift > 21) {
            return -1;
        }
    }
    *out = value;
    return 0;
}

static int send_all(int fd, const unsigned char *buf, size_t n) {
    size_t sent = 0;

    while (sent < n) {
        ssize_t s = send(fd, buf + sent, n - sent, MSG_NOSIGNAL);

        if (s <= 0) {
            return -1;
        }
        sent += (size_t)s;
    }
    return 0;
}

static void handle_publish(int fd, unsigned char header, unsigned char *body,
                           size_t len) {
    size_t off = 0;
    unsigned qos = (header >> 1) & 0x3u;

    if (len < 2) {
        return;
    }
    size_t topic_len = ((size_t)body[0] << 8) | body[1];

    off = 2;
    if (off + topic_len > len) {
        return;
    }
    const char *topic = (const char *)&body[off];

    off += topic_len;
    uint16_t packet_id = 0;

    if (qos > 0) {
        if (off + 2 > len) {
            return;
        }
        packet_id = (uint16_t)(((uint16_t)body[off] << 8) | body[off + 1]);
        off += 2;
    }
    size_t payload_len = len - off;

    printf("PUBLISH %.*s %.*s\n", (int)topic_len, topic, (int)payload_len,
           (const char *)&body[off]);
    fflush(stdout);

    if (qos == 1) {
        unsigned char ack[4] = { 0x40, 0x02, (unsigned char)(packet_id >> 8),
                                 (unsigned char)(packet_id & 0xFF) };

        send_all(fd, ack, sizeof ack);
    }
}

static int serve_client(int fd) {
    unsigned char body[MAX_PACKET];

    for (;;) {
        unsigned char header;
        size_t remaining = 0;

        if (read_full(fd, &header, 1) < 0) {
            return 0;
        }
        if (read_remaining_length(fd, &remaining) < 0) {
            return -1;
        }
        if (remaining > sizeof body) {
            return -1;
        }
        if (read_full(fd, body, remaining) < 0) {
            return -1;
        }

        switch (header >> 4) {
        case 1: { /* CONNECT → CONNACK */
            unsigned char ack[4] = { 0x20, 0x02, 0x00, 0x00 };

            printf("CONNECT\n");
            fflush(stdout);
            if (send_all(fd, ack, sizeof ack) < 0) {
                return -1;
            }
            break;
        }
        case 3: /* PUBLISH */
            handle_publish(fd, header, body, remaining);
            break;
        case 8: { /* SUBSCRIBE → SUBACK */
            if (remaining < 2) {
                return -1;
            }
            uint16_t pid = (uint16_t)(((uint16_t)body[0] << 8) | body[1]);
            unsigned char ack[MAX_PACKET];
            size_t n = 0;
            size_t i = 2;

            /* 负载是若干 (2 字节长度 + 主题 + 1 字节 QoS) */
            while (i + 2 < remaining) {
                size_t topic_len = ((size_t)body[i] << 8) | body[i + 1];

                if (i + 3 + topic_len > remaining) {
                    return -1;
                }
                ack[4 + n] = 0x00; /* 每个主题回 QoS 0 成功 */
                n++;
                i += 2 + topic_len + 1;
            }

            /* 固定头 0x90，剩余长度 = 报文标识符 2 字节 + 每个主题 1 字节 */
            ack[0] = 0x90;
            ack[1] = (unsigned char)(2 + n);
            ack[2] = (unsigned char)(pid >> 8);
            ack[3] = (unsigned char)(pid & 0xFF);
            printf("SUBSCRIBE %zu 个主题\n", n);
            fflush(stdout);
            if (send_all(fd, ack, 4 + n) < 0) {
                return -1;
            }
            break;
        }
        case 12: { /* PINGREQ → PINGRESP */
            unsigned char ack[2] = { 0xD0, 0x00 };

            send_all(fd, ack, sizeof ack);
            break;
        }
        case 14: /* DISCONNECT */
            printf("DISCONNECT\n");
            fflush(stdout);
            return 0;
        default:
            printf("忽略报文类型 0x%02x\n", header >> 4);
            fflush(stdout);
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    int port = argc > 1 ? atoi(argv[1]) : DEFAULT_PORT;
    int listen_fd;
    struct sockaddr_in addr;
    int one = 1;

    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);
    signal(SIGPIPE, SIG_IGN);

    listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listen_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        return EXIT_FAILURE;
    }
    if (listen(listen_fd, 4) < 0) {
        perror("listen");
        return EXIT_FAILURE;
    }
    printf("fake_broker 监听 127.0.0.1:%d\n", port);
    fflush(stdout);

    while (!g_stop) {
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof peer;
        int fd = accept(listen_fd, (struct sockaddr *)&peer, &peer_len);

        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }
        printf("客户端接入\n");
        fflush(stdout);
        serve_client(fd);
        close(fd);
        printf("客户端断开\n");
        fflush(stdout);
    }
    close(listen_fd);
    return EXIT_SUCCESS;
}