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

#define MAX_CONNS 64
#define MAX_LINE 1024
#define OUT_CAP (64 * 1024)

struct conn {
    int fd;
    int eof;
    int closing;
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

static void conn_close(struct conn *c) {
    close(c->fd);
    c->fd = -1;
}

/* 把输出缓冲区写出去，直到写空或者内核发送缓冲区满了。 */
static void conn_flush(struct conn *c) {
    while (c->out_len > 0) {
        ssize_t n = write(c->fd, c->out, c->out_len);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            conn_close(c);
            return;
        }
        memmove(c->out, c->out + n, c->out_len - (size_t)n);
        c->out_len -= (size_t)n;
    }
    if (c->closing || c->eof) {
        conn_close(c);
    }
}

/* 把输入缓冲区里完整的行取出来，大写后追加到输出缓冲区。输出放不下时停下。 */
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

/* 读一次，处理出完整的行，再尽量把输出写出去。 */
static void conn_read(struct conn *c) {
    if (c->in_len == MAX_LINE) {
        /* 一行超过缓冲区容量，断开。 */
        conn_close(c);
        return;
    }
    ssize_t n = read(c->fd, c->in + c->in_len, MAX_LINE - c->in_len);
    if (n == 0) {
        c->eof = 1;
        conn_flush(c);
        return;
    }
    if (n == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            conn_close(c);
        }
        return;
    }
    c->in_len += (size_t)n;
    conn_process(c);
    conn_flush(c);
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

    struct conn conns[MAX_CONNS];
    int nconns = 0;

    for (;;) {
        struct pollfd pfds[MAX_CONNS + 1];
        pfds[0].fd = listener;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        for (int i = 0; i < nconns; i++) {
            /* 输出缓冲区快满时先不读，等写出去腾出空间。 */
            short events = 0;
            if (!conns[i].eof && conns[i].out_len + MAX_LINE <= OUT_CAP) {
                events = POLLIN;
            }
            if (conns[i].out_len > 0) {
                events |= POLLOUT;
            }
            pfds[i + 1].fd = conns[i].fd;
            pfds[i + 1].events = events;
            pfds[i + 1].revents = 0;
        }

        int ready = poll(pfds, (nfds_t)nconns + 1, -1);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("poll");
        }

        int polled = nconns;

        if (pfds[0].revents & POLLIN) {
            for (;;) {
                int fd = accept(listener, NULL, NULL);
                if (fd == -1) {
                    break;
                }
                if (nconns == MAX_CONNS || set_nonblock(fd) == -1) {
                    close(fd);
                    continue;
                }
                struct conn *c = &conns[nconns++];
                memset(c, 0, sizeof *c);
                c->fd = fd;
            }
        }

        for (int i = 0; i < polled; i++) {
            short re = pfds[i + 1].revents;
            if (re == 0) {
                continue;
            }
            if (re & (POLLERR | POLLNVAL)) {
                conn_close(&conns[i]);
                continue;
            }
            if (re & (POLLIN | POLLHUP)) {
                conn_read(&conns[i]);
            }
            if (conns[i].fd != -1 && (re & POLLOUT)) {
                conn_process(&conns[i]);
                conn_flush(&conns[i]);
            }
        }

        int keep = 0;
        for (int i = 0; i < nconns; i++) {
            if (conns[i].fd != -1) {
                conns[keep++] = conns[i];
            }
        }
        nconns = keep;
    }
}