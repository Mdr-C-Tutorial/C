#define _POSIX_C_SOURCE 200809L

#include "evl.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

#define IN_BUF_SIZE 4096
#define OUT_BUF_SIZE 65536
#define IDLE_TIMEOUT_MS 10000
#define DRAIN_TIMEOUT_MS 1000
#define DEFAULT_PORT 21200

struct conn {
    struct server *srv;
    int fd;
    evl_timer *idle_timer;
    char in_buf[IN_BUF_SIZE];
    size_t in_len;
    char out_buf[OUT_BUF_SIZE];
    size_t out_len;
    size_t out_off;
    int read_eof;
    int closing;
    struct conn *prev;
    struct conn *next;
};

struct server;

struct server {
    evl_loop *loop;
    int listen_fd;
    int sig_fd;
    struct conn *conns;
    evl_timer *drain_timer;
    int idle_ms;
    int shutting_down;
};

static int64_t start_ms;

static void log_msg(const char *what) {
    printf("[%6lld ms] %s\n", (long long)(evl_now_ms() - start_ms), what);
    fflush(stdout);
}

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);

    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void conn_list_add(struct server *srv, struct conn *c) {
    c->prev = NULL;
    c->next = srv->conns;
    if (srv->conns != NULL) {
        srv->conns->prev = c;
    }
    srv->conns = c;
}

static void conn_list_remove(struct server *srv, struct conn *c) {
    if (c->prev != NULL) {
        c->prev->next = c->next;
    } else {
        srv->conns = c->next;
    }
    if (c->next != NULL) {
        c->next->prev = c->prev;
    }
}

static void conn_free(evl_loop *loop, void *arg) {
    (void)loop;
    free(arg);
}

static void conn_close(struct server *srv, struct conn *c) {
    if (c->closing) {
        return;
    }
    c->closing = 1;
    evl_del_fd(srv->loop, c->fd);
    evl_cancel_timer(srv->loop, c->idle_timer);
    c->idle_timer = NULL;
    close(c->fd);
    conn_list_remove(srv, c);
    evl_defer(srv->loop, conn_free, c);
    if (srv->shutting_down && srv->conns == NULL) {
        evl_stop(srv->loop);
    }
}

static void conn_flush(struct conn *c) {
    while (c->out_off < c->out_len) {
        ssize_t n = write(c->fd, c->out_buf + c->out_off, c->out_len - c->out_off);

        if (n > 0) {
            c->out_off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
    if (c->out_off == c->out_len) {
        c->out_len = 0;
        c->out_off = 0;
    }
}

static void conn_compact(struct conn *c) {
    if (c->out_off == 0) {
        return;
    }
    if (c->out_off == c->out_len) {
        c->out_len = 0;
        c->out_off = 0;
        return;
    }
    memmove(c->out_buf, c->out_buf + c->out_off, c->out_len - c->out_off);
    c->out_len -= c->out_off;
    c->out_off = 0;
}

static void conn_arm_write(struct server *srv, struct conn *c) {
    unsigned mask = EVL_READ;

    if (c->out_off < c->out_len) {
        mask |= EVL_WRITE;
    }
    evl_mod_fd(srv->loop, c->fd, mask);
}

static void on_idle(evl_loop *loop, evl_timer *timer, void *arg) {
    struct conn *c = arg;

    (void)loop;
    (void)timer;
    log_msg("连接空闲超时，关闭");
    conn_close(c->srv, c);
}

static void on_conn_event(evl_loop *loop, int fd, unsigned events, void *arg);

static void handle_line(struct server *srv, struct conn *c, char *line) {
    size_t len = strlen(line);

    if (len > 0 && line[len - 1] == '\r') {
        line[--len] = '\0';
    }
    if (strcmp(line, "quit") == 0) {
        log_msg("收到 quit，关闭连接");
        conn_close(srv, c);
        return;
    }
    conn_compact(c);
    if (c->out_len + len + 1 > OUT_BUF_SIZE) {
        log_msg("输出缓冲区满，关闭连接");
        conn_close(srv, c);
        return;
    }
    for (size_t i = 0; i < len; i++) {
        c->out_buf[c->out_len++] = (char)toupper((unsigned char)line[i]);
    }
    c->out_buf[c->out_len++] = '\n';
    conn_flush(c);
    conn_arm_write(srv, c);
}

static void reset_idle_timer(struct server *srv, struct conn *c) {
    if (srv->shutting_down) {
        return;
    }
    evl_cancel_timer(srv->loop, c->idle_timer);
    c->idle_timer = evl_add_timer(srv->loop, srv->idle_ms, 0, on_idle, c);
}

static int take_line(struct server *srv, struct conn *c, char *out, size_t cap) {
    (void)srv;
    for (size_t i = 0; i < c->in_len; i++) {
        if (c->in_buf[i] != '\n') {
            continue;
        }
        size_t len = i;

        if (len >= cap) {
            len = cap - 1;
        }
        memcpy(out, c->in_buf, len);
        out[len] = '\0';
        memmove(c->in_buf, c->in_buf + i + 1, c->in_len - i - 1);
        c->in_len -= i + 1;
        return 1;
    }
    return 0;
}

static void read_conn(struct server *srv, struct conn *c) {
    char line[IN_BUF_SIZE];

    for (;;) {
        while (take_line(srv, c, line, sizeof line)) {
            handle_line(srv, c, line);
            if (c->closing) {
                return;
            }
        }
        if (c->in_len == IN_BUF_SIZE) {
            log_msg("行太长，关闭连接");
            conn_close(srv, c);
            return;
        }
        ssize_t n = read(c->fd, c->in_buf + c->in_len, IN_BUF_SIZE - c->in_len);

        if (n > 0) {
            c->in_len += (size_t)n;
            continue;
        }
        if (n == 0) {
            c->read_eof = 1;
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            log_msg("读出错，关闭连接");
            conn_close(srv, c);
            return;
        }
        break;
    }
    if (c->read_eof) {
        if (c->out_len == 0) {
            log_msg("对端关闭");
            conn_close(srv, c);
        } else {
            log_msg("对端关闭，等待输出发完");
            conn_arm_write(srv, c);
        }
        return;
    }
    reset_idle_timer(srv, c);
}

static void on_conn_event(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct conn *c = arg;
    struct server *srv = c->srv;

    (void)loop;
    (void)fd;
    if (events & EVL_READ) {
        read_conn(srv, c);
        if (c->closing) {
            return;
        }
    }
    if (events & EVL_WRITE) {
        conn_flush(c);
        if (c->closing) {
            return;
        }
        if ((srv->shutting_down || c->read_eof) && c->out_len == 0) {
            log_msg("输出已发完，关闭连接");
            conn_close(srv, c);
            return;
        }
        conn_arm_write(srv, c);
    }
}

static void on_accept(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct server *srv = arg;

    (void)loop;
    (void)events;
    for (;;) {
        int client = accept(fd, NULL, NULL);

        if (client < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("accept");
            }
            return;
        }
        if (set_nonblock(client) < 0 || srv->shutting_down) {
            close(client);
            return;
        }
        struct conn *c = calloc(1, sizeof *c);

        if (c == NULL) {
            close(client);
            return;
        }
        c->srv = srv;
        c->fd = client;
        conn_list_add(srv, c);
        if (evl_add_fd(srv->loop, client, EVL_READ, on_conn_event, c) < 0) {
            perror("evl_add_fd");
            conn_close(srv, c);
            return;
        }
        c->idle_timer = evl_add_timer(srv->loop, srv->idle_ms, 0, on_idle, c);
        log_msg("接受新连接");
    }
}

static void on_drain_timeout(evl_loop *loop, evl_timer *timer, void *arg) {
    struct server *srv = arg;

    (void)loop;
    (void)timer;
    log_msg("等待输出超时，强制关闭剩余连接");
    while (srv->conns != NULL) {
        conn_close(srv, srv->conns);
    }
}

static void begin_shutdown(struct server *srv) {
    if (srv->shutting_down) {
        return;
    }
    srv->shutting_down = 1;
    int count = 0;

    for (struct conn *c = srv->conns; c != NULL; c = c->next) {
        count++;
    }
    printf("[%6lld ms] 开始优雅退出：停止接受新连接，剩余 %d 个连接\n",
           (long long)(evl_now_ms() - start_ms), count);
    fflush(stdout);
    evl_del_fd(srv->loop, srv->listen_fd);
    close(srv->listen_fd);
    srv->listen_fd = -1;
    for (struct conn *c = srv->conns; c != NULL;) {
        struct conn *next = c->next;

        if (c->out_len == 0) {
            conn_close(srv, c);
        } else {
            conn_arm_write(srv, c);
        }
        c = next;
    }
    if (srv->conns == NULL) {
        evl_stop(srv->loop);
        return;
    }
    srv->drain_timer = evl_add_timer(srv->loop, DRAIN_TIMEOUT_MS, 0, on_drain_timeout, srv);
}

static void on_signal(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct server *srv = arg;
    struct signalfd_siginfo info;

    (void)loop;
    (void)events;
    while (read(fd, &info, sizeof info) == sizeof info) {
        if (info.ssi_signo == SIGTERM) {
            log_msg("收到 SIGTERM");
        } else if (info.ssi_signo == SIGINT) {
            log_msg("收到 SIGINT");
        }
        begin_shutdown(srv);
    }
}

static int open_listener(int port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (fd < 0) {
        perror("socket");
        return -1;
    }
    int one = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, SOMAXCONN) < 0 || set_nonblock(fd) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

static int open_signal_fd(void) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        perror("sigprocmask");
        return -1;
    }
    return signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    struct server srv = {0};

    if (argc > 1) {
        port = atoi(argv[1]);
    }
    srv.idle_ms = argc > 2 ? atoi(argv[2]) : IDLE_TIMEOUT_MS;
    start_ms = evl_now_ms();
    srv.loop = evl_create();
    if (srv.loop == NULL) {
        perror("evl_create");
        return 1;
    }
    srv.listen_fd = open_listener(port);
    srv.sig_fd = open_signal_fd();
    if (srv.listen_fd < 0 || srv.sig_fd < 0) {
        return 1;
    }
    if (evl_add_fd(srv.loop, srv.listen_fd, EVL_READ, on_accept, &srv) < 0 ||
        evl_add_fd(srv.loop, srv.sig_fd, EVL_READ, on_signal, &srv) < 0) {
        perror("evl_add_fd");
        return 1;
    }
    printf("lecho_evl 监听 127.0.0.1:%d，pid = %ld\n", port, (long)getpid());
    fflush(stdout);
    if (evl_run(srv.loop) < 0) {
        perror("evl_run");
        return 1;
    }
    log_msg("循环结束");
    evl_destroy(srv.loop);
    close(srv.sig_fd);
    return 0;
}