#define _POSIX_C_SOURCE 200809L

#include "net.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static struct addrinfo *resolve(const char *host, const char *port,
                                int flags) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     /* IPv4 或 IPv6，由 host 决定 */
    hints.ai_socktype = SOCK_STREAM; /* TCP */
    hints.ai_flags = flags;

    struct addrinfo *list = NULL;
    int rc = getaddrinfo(host, port, &hints, &list);
    if (rc != 0) {
        fprintf(stderr, "cannot resolve %s:%s: %s\n", host, port,
                gai_strerror(rc));
        return NULL;
    }
    return list;
}

int net_listen(const char *host, const char *port) {
    struct addrinfo *list = resolve(host, port, AI_PASSIVE);
    if (list == NULL) {
        return -1;
    }
    int fd = -1;
    int err = 0;
    /* 一个主机名可能对应多个地址（例如 localhost 对应 ::1 和 127.0.0.1），
     * 依次尝试，用第一个成功的。 */
    for (struct addrinfo *ai = list; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            err = errno;
            continue;
        }
        /* 服务器刚停下又马上重启时，上次的连接可能还处在 TIME_WAIT 状态，
         * 不设这个选项，bind 会以 EADDRINUSE 失败。 */
        int yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == 0 &&
            bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 &&
            listen(fd, SOMAXCONN) == 0) {
            break;
        }
        err = errno;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(list);
    if (fd < 0) {
        fprintf(stderr, "cannot listen on %s:%s: %s\n", host, port,
                strerror(err));
    }
    return fd;
}

int net_connect(const char *host, const char *port) {
    struct addrinfo *list = resolve(host, port, 0);
    if (list == NULL) {
        return -1;
    }
    int fd = -1;
    int err = 0;
    for (struct addrinfo *ai = list; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            err = errno;
            continue;
        }
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;
        }
        err = errno;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(list);
    if (fd < 0) {
        fprintf(stderr, "cannot connect to %s:%s: %s\n", host, port,
                strerror(err));
    }
    return fd;
}

int net_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ? -1 : 0;
}

int net_send_all(int fd, const void *data, size_t len) {
    const unsigned char *p = data;
    while (len > 0) {
        /* 即使是阻塞套接字，send 也可能只交出一部分（例如被信号打断），
         * 所以要循环到全部发完。 */
        ssize_t n = send(fd, p, len, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

void net_peer_name(int fd, char *out, size_t cap) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof addr;
    char host[64];
    char port[16];
    if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == 0 &&
        getnameinfo((struct sockaddr *)&addr, addr_len, host, sizeof host,
                    port, sizeof port, NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
        snprintf(out, cap, "%s:%s", host, port);
    } else {
        snprintf(out, cap, "?");
    }
}
