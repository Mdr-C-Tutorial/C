#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* 读满 len 个字节；对端提前关闭时返回已读字节数。 */
static int read_full(int fd, void *buf, size_t len) {
    char *p = buf;
    size_t got = 0;

    while (got < len) {
        ssize_t n = read(fd, p + got, len - got);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            break;
        }
        got += (size_t)n;
    }
    return (int)got;
}

/* 发送一条消息：4 字节长度前缀（网络字节序）加负载。 */
static int send_msg(int fd, const void *payload, size_t len) {
    uint32_t be = htonl((uint32_t)len);
    if (write(fd, &be, sizeof be) != (ssize_t)sizeof be) {
        return -1;
    }
    const char *p = payload;
    size_t left = len;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    return 0;
}

/* 接收一条消息；调用者提供缓冲区并给出容量。成功返回负载长度。 */
static int recv_msg(int fd, char *buf, size_t cap) {
    uint32_t be = 0;
    if (read_full(fd, &be, sizeof be) != (int)sizeof be) {
        return -1;
    }
    size_t len = ntohl(be);
    if (len >= cap) {
        return -1;
    }
    if (read_full(fd, buf, len) != (int)len) {
        return -1;
    }
    buf[len] = '\0';
    return (int)len;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void server(int lfd) {
    int cfd = accept(lfd, NULL, NULL);
    if (cfd < 0) {
        die("accept");
    }

    char line[256];
    for (;;) {
        int n = recv_msg(cfd, line, sizeof line);
        if (n < 0) {
            puts("server: peer closed");
            break;
        }
        printf("server got %d bytes: %s\n", n, line);
    }
    close(cfd);
}

static void client(const struct sockaddr_in *addr) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket");
    }
    if (connect(fd, (const struct sockaddr *)addr, sizeof *addr) < 0) {
        die("connect");
    }

    const char *msgs[] = {"first", "a longer second message", "third"};
    for (size_t i = 0; i < sizeof msgs / sizeof msgs[0]; i++) {
        if (send_msg(fd, msgs[i], strlen(msgs[i])) < 0) {
            die("send_msg");
        }
    }
    close(fd);
}

int main(void) {
    int lfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (lfd < 0) {
        die("socket");
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        die("bind");
    }
    if (listen(lfd, 1) < 0) {
        die("listen");
    }
    socklen_t len = sizeof addr;
    if (getsockname(lfd, (struct sockaddr *)&addr, &len) < 0) {
        die("getsockname");
    }

    pid_t pid = fork();
    if (pid < 0) {
        die("fork");
    }
    if (pid == 0) {
        client(&addr);
        _exit(EXIT_SUCCESS);
    }

    server(lfd);
    close(lfd);
    return EXIT_SUCCESS;
}