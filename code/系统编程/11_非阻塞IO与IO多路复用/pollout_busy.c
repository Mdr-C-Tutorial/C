#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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

static volatile sig_atomic_t stop = 0;

static void on_alarm(int signo) {
    (void)signo;
    stop = 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法：%s 端口 always|ondemand\n", argv[0]);
        return EXIT_FAILURE;
    }
    int eager = strcmp(argv[2], "always") == 0;
    int listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listener == -1) {
        die("socket");
    }
    int one = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) ==
        -1) {
        die("setsockopt");
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)atoi(argv[1])),
    };
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        die("inet_pton");
    }
    if (bind(listener, (struct sockaddr *)&addr, sizeof addr) == -1) {
        die("bind");
    }
    if (listen(listener, 8) == -1) {
        die("listen");
    }
    int conn = accept(listener, NULL, NULL);
    if (conn == -1) {
        die("accept");
    }
    if (set_nonblock(conn) == -1) {
        die("fcntl");
    }

    struct sigaction sa = {.sa_handler = on_alarm};
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        die("sigaction");
    }

    alarm(1);
    unsigned long calls = 0;
    while (!stop) {
        calls++;
        struct pollfd pfds[2] = {
            {.fd = listener, .events = POLLIN},
            {.fd = conn, .events = POLLIN},
        };
        if (eager) {
            pfds[1].events |= POLLOUT;
        }
        int ready = poll(pfds, 2, -1);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("poll");
        }
    }
    printf("%s：1 秒内调用 poll %lu 次\n", eager ? "一直关注 POLLOUT" : "按需关注 POLLOUT", calls);
    return 0;
}