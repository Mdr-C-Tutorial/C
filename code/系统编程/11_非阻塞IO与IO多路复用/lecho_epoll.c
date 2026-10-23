#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CONNS 64
#define MAX_LINE 1024
#define OUT_CAP (64 * 1024)

struct conn {
    int fd;
    int eof;
    int closing;
    int mask;
    char in[MAX_LINE];
    size_t in_len;
    char out[OUT_CAP];
    size_t out_len;
};

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

static void conn_close(int ep, struct conn *c) {
    epoll_ctl(ep, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    c->fd = -1;
}

/* 让兴趣列表跟上「输出缓冲区有没有空间」的变化。 */
static void conn_update(int ep, struct conn *c) {
    uint32_t mask = 0;
    if (!c->eof && c->out_len + MAX_LINE <= OUT_CAP) {
        mask |= EPOLLIN;
    }
    if (c->out_len > 0) {
        mask |= EPOLLOUT;
    }
    if ((int)mask == c->mask) {
        return;
    }
    struct epoll_event ev = {
        .events = mask,
        .data.ptr = c,
    };
    if (epoll_ctl(ep, EPOLL_CTL_MOD, c->fd, &ev) == -1) {
        die("epoll_ctl");
    }
    c->mask = (int)mask;
}

/* 把输出缓冲区写出去，直到写空或者内核发送缓冲区满了。 */
static void conn_flush(int ep, struct conn *c) {
    while (c->out_len > 0) {
        ssize_t n = write(c->fd, c->out, c->out_len);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            conn_close(ep, c);
            return;
        }
        memmove(c->out, c->out + n, c->out_len - (size_t)n);
        c->out_len -= (size_t)n;
    }
    if (c->out_len == 0 && (c->closing || c->eof)) {
        conn_close(ep, c);
        return;
    }
    conn_update(ep, c);
}

/* 把输入缓冲区里完整的行取出来，大写后追加到输出缓冲区。 */
static void conn_process(struct conn *c) {
    size_t start = 0;
    while (c->fd != -1 && !c->closing) {
        char *nl = memchr(c->in + start, '\n', c->in_len - start);
        if (nl == NULL) {
            break;
        }
        size_t len = (size_t)(nl - (c->in + start)) + 1;
        if (len == 5 && memcmp(c->in + start, "quit\n", 5) == 0) {
            c->closing = 1;
            start += len;
            break;
        }
        if (c->out_len + len > OUT_CAP) {
            break;
        }
        for (size_t i = 0; i < len; i++) {
            char ch = c->in[start + i];
            c->out[c->out_len + i] =
                (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
        }
        c->out_len += len;
        start += len;
    }
    memmove(c->in, c->in + start, c->in_len - start);
    c->in_len -= start;
}

/* 读到 EAGAIN 为止，每一块都解析出完整的行。 */
static void conn_read(int ep, struct conn *c) {
    for (;;) {
        if (c->in_len == MAX_LINE) {
            /* 一行超过缓冲区容量，断开。 */
            conn_close(ep, c);
            return;
        }
        ssize_t n = read(c->fd, c->in + c->in_len, MAX_LINE - c->in_len);
        if (n == 0) {
            c->eof = 1;
            break;
        }
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            conn_close(ep, c);
            return;
        }
        c->in_len += (size_t)n;
        conn_process(c);
        if (c->fd == -1 || c->closing || c->out_len + MAX_LINE > OUT_CAP) {
            break;
        }
    }
    if (c->fd != -1) {
        conn_flush(ep, c);
    }
}

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : "21100";
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        die("signal");
    }
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
        .sin_port = htons((uint16_t)atoi(port)),
    };
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        die("inet_pton");
    }
    if (bind(listener, (struct sockaddr *)&addr, sizeof addr) == -1) {
        die("bind");
    }
    if (listen(listener, 16) == -1) {
        die("listen");
    }
    if (set_nonblock(listener) == -1) {
        die("fcntl");
    }

    int ep = epoll_create1(EPOLL_CLOEXEC);
    if (ep == -1) {
        die("epoll_create1");
    }
    struct epoll_event lev = {
        .events = EPOLLIN,
        .data.ptr = NULL,
    };
    if (epoll_ctl(ep, EPOLL_CTL_ADD, listener, &lev) == -1) {
        die("epoll_ctl");
    }

    int nconns = 0;
    struct epoll_event events[16];

    for (;;) {
        int ready = epoll_wait(ep, events, 16, -1);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("epoll_wait");
        }

        for (int i = 0; i < ready; i++) {
            struct conn *c = events[i].data.ptr;
            uint32_t re = events[i].events;
            if (c == NULL) {
                for (;;) {
                    int fd = accept(listener, NULL, NULL);
                    if (fd == -1) {
                        break;
                    }
                    if (nconns == MAX_CONNS || set_nonblock(fd) == -1) {
                        close(fd);
                        continue;
                    }
                    c = calloc(1, sizeof *c);
                    if (c == NULL) {
                        close(fd);
                        continue;
                    }
                    c->fd = fd;
                    struct epoll_event cev = {
                        .events = EPOLLIN,
                        .data.ptr = c,
                    };
                    if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &cev) == -1) {
                        die("epoll_ctl");
                    }
                    c->mask = EPOLLIN;
                    nconns++;
                }
                continue;
            }

            if (re & (EPOLLERR | EPOLLHUP)) {
                c->eof = 1;
            }
            if (re & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
                conn_read(ep, c);
                if (c->fd == -1) {
                    nconns--;
                    continue;
                }
            }
            if (re & EPOLLOUT) {
                conn_process(c);
                conn_flush(ep, c);
                if (c->fd == -1) {
                    nconns--;
                }
            }
        }
    }
}