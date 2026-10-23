#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static double now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        die("clock_gettime");
    }
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "用法：%s HOST PORT TIMEOUT_MS\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *host = argv[1];
    const char *port = argv[2];
    int timeout_ms = atoi(argv[3]);

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_NUMERICSERV,
    };
    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo：%s\n", gai_strerror(rc));
        return EXIT_FAILURE;
    }

    int fd = socket(res->ai_family, res->ai_socktype | SOCK_CLOEXEC,
                    res->ai_protocol);
    if (fd == -1) {
        die("socket");
    }
    if (set_nonblock(fd) == -1) {
        die("fcntl");
    }

    double start = now_ms();
    rc = connect(fd, res->ai_addr, res->ai_addrlen);
    if (rc == -1 && errno != EINPROGRESS) {
        printf("%s:%s 失败：%s，耗时 %.3f ms\n", host, port, strerror(errno),
               now_ms() - start);
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }
    freeaddrinfo(res);

    if (rc == -1) {
        struct pollfd pfd = {
            .fd = fd,
            .events = POLLOUT,
        };
        rc = poll(&pfd, 1, timeout_ms);
        if (rc == 0) {
            printf("%s:%s 超时（%d ms）\n", host, port, timeout_ms);
            close(fd);
            return EXIT_FAILURE;
        }
        if (rc == -1) {
            if (errno == EINTR) {
                printf("%s:%s 被信号中断\n", host, port);
                close(fd);
                return EXIT_FAILURE;
            }
            die("poll");
        }
        int err = 0;
        socklen_t len = sizeof err;
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1) {
            die("getsockopt");
        }
        if (err != 0) {
            printf("%s:%s 失败：%s，耗时 %.3f ms\n", host, port, strerror(err),
                   now_ms() - start);
            close(fd);
            return EXIT_FAILURE;
        }
    }

    printf("%s:%s 连接成功，耗时 %.3f ms\n", host, port, now_ms() - start);
    close(fd);
    return 0;
}