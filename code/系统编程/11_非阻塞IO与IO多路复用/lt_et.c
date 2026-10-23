#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
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

static int make_pipe(int p[2], int ep, uint32_t flags) {
    if (pipe(p) == -1) {
        die("pipe");
    }
    if (set_nonblock(p[0]) == -1) {
        die("fcntl");
    }
    char chunk[512];
    memset(chunk, 'a', sizeof chunk);
    for (int i = 0; i < 4; i++) {
        if (write(p[1], chunk, sizeof chunk) != sizeof chunk) {
            die("write");
        }
    }
    struct epoll_event ev = {
        .events = EPOLLIN | flags,
        .data.fd = p[0],
    };
    if (epoll_ctl(ep, EPOLL_CTL_ADD, p[0], &ev) == -1) {
        die("epoll_ctl");
    }
    return ep;
}

static void read_once(int fd, ssize_t *total) {
    char buf[512];
    ssize_t n = read(fd, buf, sizeof buf);
    if (n > 0) {
        *total += n;
    }
}

int main(void) {
    char buf[512];
    struct epoll_event out[4];
    int ep = epoll_create1(EPOLL_CLOEXEC);
    if (ep == -1) {
        die("epoll_create1");
    }

    int p[2];
    make_pipe(p, ep, 0);
    printf("水平触发：第一次 epoll_wait 返回 %d\n", epoll_wait(ep, out, 4, 500));
    ssize_t got = 0;
    read_once(p[0], &got);
    printf("水平触发：只读走 %zd 字节，还剩 %d 字节\n", got, 2048 - (int)got);
    printf("水平触发：第二次 epoll_wait 返回 %d\n\n",
           epoll_wait(ep, out, 4, 500));
    close(ep);
    close(p[0]);
    close(p[1]);

    ep = epoll_create1(EPOLL_CLOEXEC);
    make_pipe(p, ep, EPOLLET);
    printf("边缘触发：第一次 epoll_wait 返回 %d\n", epoll_wait(ep, out, 4, 500));
    got = 0;
    read_once(p[0], &got);
    printf("边缘触发：只读走 %zd 字节，还剩 %d 字节\n", got, 2048 - (int)got);
    printf("边缘触发：第二次 epoll_wait 返回 %d\n\n",
           epoll_wait(ep, out, 4, 500));
    close(ep);
    close(p[0]);
    close(p[1]);

    ep = epoll_create1(EPOLL_CLOEXEC);
    make_pipe(p, ep, EPOLLET);
    printf("边缘触发并读空：第一次 epoll_wait 返回 %d\n",
           epoll_wait(ep, out, 4, 500));
    got = 0;
    for (;;) {
        ssize_t n = read(p[0], buf, sizeof buf);
        if (n == -1) {
            break;
        }
        got += n;
    }
    printf("边缘触发并读空：读到 EAGAIN，共 %zd 字节\n", got);
    if (write(p[1], "more", 4) != 4) {
        die("write");
    }
    printf("边缘触发并读空：再写入 4 字节，epoll_wait 返回 %d\n",
           epoll_wait(ep, out, 4, 500));
    close(ep);
    close(p[0]);
    close(p[1]);
    return 0;
}