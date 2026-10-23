/* EMFILE 演示：先把本进程的描述符额度用光，再让客户端连进来。
 * 用法：./emfile naive|reserve
 * 两种模式都在水平触发的 epoll 上等待监听套接字。
 * reserve 模式在填满额度前额外留一个描述符作备用。 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 21401

int main(int argc, char *argv[]) {
    int reserve_mode;
    struct rlimit rl;
    int listener;
    int ep;
    int one = 1;
    int reserve = -1;
    int filled = 0;
    struct sockaddr_in addr;
    struct epoll_event ev;
    struct epoll_event events[4];

    if (argc != 2 || (strcmp(argv[1], "naive") != 0 &&
                      strcmp(argv[1], "reserve") != 0)) {
        fprintf(stderr, "usage: %s naive|reserve\n", argv[0]);
        return EXIT_FAILURE;
    }
    reserve_mode = strcmp(argv[1], "reserve") == 0;

    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("getrlimit");
        return EXIT_FAILURE;
    }
    rl.rlim_cur = 20;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("setrlimit");
        return EXIT_FAILURE;
    }

    listener = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listener == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (struct sockaddr *)&addr, sizeof addr) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }
    if (listen(listener, 16) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }
    ep = epoll_create1(EPOLL_CLOEXEC);
    if (ep == -1) {
        perror("epoll_create1");
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof ev);
    ev.events = EPOLLIN;
    ev.data.fd = listener;
    if (epoll_ctl(ep, EPOLL_CTL_ADD, listener, &ev) == -1) {
        perror("epoll_ctl");
        return EXIT_FAILURE;
    }

    unsigned long failures = 0;

    if (reserve_mode) {
        reserve = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (reserve == -1) {
            perror("open reserve");
            return EXIT_FAILURE;
        }
    }
    /* 把剩下的额度填满。naive 模式连一个空位都不留。 */
    while (open("/dev/null", O_RDONLY | O_CLOEXEC) != -1) {
        filled++;
    }
    printf("%s: filled %d descriptors, limit soft=%llu\n", argv[1], filled,
           (unsigned long long)rl.rlim_cur);
    fflush(stdout);

    for (;;) {
        int n = epoll_wait(ep, events, 4, 1000);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            return EXIT_FAILURE;
        }
        for (int i = 0; i < n; i++) {
            int fd;

            if (events[i].data.fd != listener) {
                continue;
            }
            fd = accept(listener, NULL, NULL);
            if (fd == -1) {
                if (errno != EMFILE) {
                    continue;
                }
                failures++;
                if (failures == 1) {
                    fprintf(stderr, "accept: %s\n", strerror(errno));
                }
                if (reserve_mode && reserve >= 0) {
                    /* 腾出预留位，接受这个连接并立刻拒绝它 */
                    close(reserve);
                    fd = accept(listener, NULL, NULL);
                    if (fd >= 0) {
                        static const char busy[] = "server busy\n";
                        write(fd, busy, sizeof busy - 1);
                        close(fd);
                    }
                    reserve = open("/dev/null", O_RDONLY | O_CLOEXEC);
                }
                continue;
            }
            close(fd);
        }
    }
}