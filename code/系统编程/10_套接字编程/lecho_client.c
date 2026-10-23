#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define LINE_CAP 4096

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
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

/* 客户端维护自己的输入缓冲区：一次 recv 可能带回好几个半行或整行。 */
struct line_reader {
    int fd;
    char buf[LINE_CAP];
    size_t start; /* 下一个未处理的字节 */
    size_t end;   /* 已收到数据的末尾 */
};

/* 取一行，去掉行尾换行；连接关闭或出错时返回 -1。 */
static int next_line(struct line_reader *lr, char *out, size_t cap) {
    for (;;) {
        for (size_t i = lr->start; i < lr->end; i++) {
            if (lr->buf[i] != '\n') {
                continue;
            }
            size_t len = i - lr->start;
            if (len >= cap) {
                len = cap - 1;
            }
            memcpy(out, lr->buf + lr->start, len);
            out[len] = '\0';
            lr->start = i + 1;
            return (int)len;
        }

        if (lr->start > 0) {
            memmove(lr->buf, lr->buf + lr->start, lr->end - lr->start);
            lr->end -= lr->start;
            lr->start = 0;
        }
        if (lr->end == sizeof lr->buf) {
            return -1;
        }

        ssize_t n = recv(lr->fd, lr->buf + lr->end, sizeof lr->buf - lr->end, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        lr->end += (size_t)n;
    }
}

static int connect_to(const char *host, const char *port) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    int fd = -1;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype | SOCK_CLOEXEC,
                    p->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        fprintf(stderr, "could not connect to %s port %s\n", host, port);
        exit(EXIT_FAILURE);
    }
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s HOST PORT\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct line_reader lr = {0};
    lr.fd = connect_to(argv[1], argv[2]);

    char line[LINE_CAP];
    while (fgets(line, sizeof line, stdin) != NULL) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        if (write_all(lr.fd, line, strlen(line)) < 0 ||
            write_all(lr.fd, "\n", 1) < 0) {
            die("write");
        }
        char reply[LINE_CAP];
        if (next_line(&lr, reply, sizeof reply) < 0) {
            puts("connection closed");
            break;
        }
        printf("echo: %s\n", reply);
    }

    close(lr.fd);
    return EXIT_SUCCESS;
}