#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
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

/* SIGCHLD 处理函数：信号会合并，所以一次信号要尽量回收所有已退出的子进程。 */
static void reap_children(int signo) {
    (void)signo;
    int saved = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    errno = saved;
}

static int tcp_listen(const char *port) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo("127.0.0.1", port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    int lfd = -1;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        lfd = socket(p->ai_family, p->ai_socktype | SOCK_CLOEXEC,
                     p->ai_protocol);
        if (lfd < 0) {
            continue;
        }
        int one = 1;
        if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
            die("setsockopt");
        }
        if (bind(lfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(lfd);
        lfd = -1;
    }
    freeaddrinfo(res);

    if (lfd < 0) {
        die("bind");
    }
    if (listen(lfd, 16) < 0) {
        die("listen");
    }
    return lfd;
}

static void handle_connection(int fd) {
    char buf[LINE_CAP];
    size_t used = 0;

    for (;;) {
        ssize_t n = recv(fd, buf + used, sizeof buf - used, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        if (n == 0) {
            return;
        }
        used += (size_t)n;

        size_t start = 0;
        for (size_t i = 0; i < used; i++) {
            if (buf[i] != '\n') {
                continue;
            }
            buf[i] = '\0';
            if (strcmp(buf + start, "quit") == 0) {
                return;
            }
            for (char *q = buf + start; *q != '\0'; q++) {
                *q = (char)toupper((unsigned char)*q);
            }
            if (write_all(fd, buf + start, strlen(buf + start)) < 0 ||
                write_all(fd, "\n", 1) < 0) {
                return;
            }
            start = i + 1;
        }
        memmove(buf, buf + start, used - start);
        used -= start;
        if (used == sizeof buf) {
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : "21000";

    signal(SIGPIPE, SIG_IGN);

    struct sigaction sa = {0};
    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        die("sigaction");
    }

    int lfd = tcp_listen(port);
    printf("listening on 127.0.0.1 port %s (one process per connection)\n",
           port);
    fflush(stdout);

    for (;;) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            die("accept");
        }

        pid_t pid = fork();
        if (pid < 0) {
            die("fork");
        }
        if (pid == 0) {
            /* 子进程不需要监听套接字：关掉它，否则父进程退出后端口仍被占用。 */
            close(lfd);
            handle_connection(cfd);
            close(cfd);
            exit(EXIT_SUCCESS);
        }

        /* 父进程不再关心这个连接，关掉自己这份描述符。 */
        close(cfd);
    }
}