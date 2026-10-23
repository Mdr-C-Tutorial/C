/* lecho 加固版：连接数上限、单行上限、输出缓冲上限、空闲超时、
 * SIGTERM 优雅退出、SIGHUP 重载配置。单线程 epoll（水平触发）。 */
#define _GNU_SOURCE /* accept4、signalfd、close_range 等 Linux 扩展 */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PORT 21400
#define GRACE_MS 3000
#define LOOP_TICK_MS 200

struct config {
    long max_conn;
    long max_line;
    long max_out;
    long idle_ms;
    char banner[128];
};

struct conn {
    int fd;
    int64_t last_ms;
    char *in;
    size_t in_len;
    size_t in_cap;
    char *out;
    size_t out_len;
    size_t out_cap;
    int close_after_flush;
};

static struct config cfg = {
    .max_conn = 8,
    .max_line = 4096,
    .max_out = 65536,
    .idle_ms = 10000,
    .banner = "lecho ready",
};

static struct conn **conns;
static size_t conn_count;
static size_t conn_cap;

static int listener = -1;
static int epfd = -1;
static int sig_fd = -1;
static int reserve_fd = -1;
static int listener_tag;
static int signal_tag;

static int draining;
static int64_t drain_deadline_ms;
static const char *config_path;

static void log_msg(const char *level, const char *fmt, ...) {
    struct timespec ts;
    struct tm tm;
    char stamp[32];
    va_list ap;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", &tm);
    fprintf(stderr, "%s.%03ld %-5s ", stamp, ts.tv_nsec / 1000000, level);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

static int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int parse_long(const char *text, long *out) {
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return -1;
    }
    *out = value;
    return 0;
}

/* 读取并校验整份配置。只有全部合法时才写入 out，
 * 这样重载失败时旧配置原封不动。 */
static int load_config(const char *path, struct config *out) {
    struct config next = *out;
    char line[256];
    int lineno = 0;
    int ok = 1;
    FILE *fp = fopen(path, "re");

    if (fp == NULL) {
        log_msg("ERROR", "config %s: %s", path, strerror(errno));
        return -1;
    }
    while (ok && fgets(line, sizeof line, fp) != NULL) {
        char *key;
        char *value;
        char *sep;
        long number;

        lineno++;
        line[strcspn(line, "\r\n")] = '\0';
        key = line;
        while (*key == ' ' || *key == '\t') {
            key++;
        }
        if (*key == '\0' || *key == '#') {
            continue;
        }
        sep = strpbrk(key, " \t");
        if (sep == NULL) {
            log_msg("ERROR", "config line %d: missing value", lineno);
            ok = 0;
            break;
        }
        *sep = '\0';
        value = sep + 1;
        while (*value == ' ' || *value == '\t') {
            value++;
        }
        if (strcmp(key, "max_conn") == 0 || strcmp(key, "max_line") == 0 ||
            strcmp(key, "max_out") == 0 || strcmp(key, "idle_ms") == 0) {
            if (parse_long(value, &number) == -1 || number <= 0) {
                log_msg("ERROR", "config line %d: bad number", lineno);
                ok = 0;
                break;
            }
            if (strcmp(key, "max_conn") == 0) {
                next.max_conn = number;
            } else if (strcmp(key, "max_line") == 0) {
                next.max_line = number;
            } else if (strcmp(key, "max_out") == 0) {
                next.max_out = number;
            } else {
                next.idle_ms = number;
            }
        } else if (strcmp(key, "banner") == 0) {
            snprintf(next.banner, sizeof next.banner, "%s", value);
        } else {
            log_msg("ERROR", "config line %d: unknown key %s", lineno, key);
            ok = 0;
        }
    }
    fclose(fp);
    if (!ok) {
        return -1;
    }
    if (next.max_conn > 100000 || next.max_line > 1 << 24 ||
        next.max_out > 1 << 28 || next.idle_ms > 86400000) {
        log_msg("ERROR", "config: value out of range");
        return -1;
    }
    *out = next;
    return 0;
}

static int buf_reserve(char **buf, size_t *cap, size_t need) {
    size_t want = *cap == 0 ? 64 : *cap;
    char *grown;

    if (need <= *cap) {
        return 0;
    }
    while (want < need) {
        want *= 2;
    }
    grown = realloc(*buf, want);
    if (grown == NULL) {
        return -1;
    }
    *buf = grown;
    *cap = want;
    return 0;
}

static void conn_set_events(struct conn *c, uint32_t events) {
    struct epoll_event ev;

    memset(&ev, 0, sizeof ev);
    ev.events = events;
    ev.data.ptr = c;
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev) == -1) {
        log_msg("ERROR", "epoll_ctl MOD fd=%d: %s", c->fd, strerror(errno));
    }
}

static void conn_update_events(struct conn *c) {
    if (draining) {
        conn_set_events(c, EPOLLOUT);
    } else if (c->out_len > 0 || c->close_after_flush) {
        conn_set_events(c, EPOLLIN | EPOLLOUT);
    } else {
        conn_set_events(c, EPOLLIN);
    }
}

static void conn_close(struct conn *c, const char *reason) {
    log_msg("INFO", "close fd=%d (%s)", c->fd, reason);
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    free(c->in);
    free(c->out);
    for (size_t i = 0; i < conn_count; i++) {
        if (conns[i] == c) {
            conns[i] = conns[conn_count - 1];
            conn_count--;
            break;
        }
    }
    free(c);
}

static int conn_queue(struct conn *c, const char *data, size_t len) {
    if (c->out_len + len > (size_t)cfg.max_out) {
        log_msg("WARN", "fd=%d output over %ld bytes, closing", c->fd,
                cfg.max_out);
        conn_close(c, "slow reader");
        return -1;
    }
    if (buf_reserve(&c->out, &c->out_cap, c->out_len + len) == -1) {
        log_msg("ERROR", "out of memory");
        conn_close(c, "out of memory");
        return -1;
    }
    memcpy(c->out + c->out_len, data, len);
    c->out_len += len;
    return 0;
}

/* 返回 0 表示连接已关闭。 */
static int conn_write(struct conn *c) {
    while (c->out_len > 0) {
        ssize_t n = send(c->fd, c->out, c->out_len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            conn_close(c, strerror(errno));
            return 0;
        }
        memmove(c->out, c->out + n, c->out_len - (size_t)n);
        c->out_len -= (size_t)n;
    }
    if (c->out_len == 0 && c->close_after_flush) {
        conn_close(c, "done");
        return 0;
    }
    conn_update_events(c);
    return 1;
}

/* 返回 0 表示连接已关闭。 */
static int process_line(struct conn *c, const char *line, size_t len) {
    char *upper;
    size_t i;

    if (len == 4 && memcmp(line, "quit", 4) == 0) {
        c->close_after_flush = 1;
        return 1;
    }
    upper = malloc(len + 1);
    if (upper == NULL) {
        conn_close(c, "out of memory");
        return 0;
    }
    for (i = 0; i < len; i++) {
        upper[i] = (char)toupper((unsigned char)line[i]);
    }
    upper[len] = '\n';
    if (conn_queue(c, upper, len + 1) == -1) {
        free(upper);
        return 0;
    }
    free(upper);
    return 1;
}

/* 返回 0 表示连接已关闭。 */
static int conn_read(struct conn *c) {
    char buf[4096];

    for (;;) {
        ssize_t n = read(c->fd, buf, sizeof buf);
        if (n == 0) {
            if (c->out_len > 0) {
                /* 对端发送完毕但还在读，先把排队的输出写完再关。 */
                c->close_after_flush = 1;
                break;
            }
            conn_close(c, "peer closed");
            return 0;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            conn_close(c, strerror(errno));
            return 0;
        }
        c->last_ms = now_ms();
        if (buf_reserve(&c->in, &c->in_cap, c->in_len + (size_t)n) == -1) {
            conn_close(c, "out of memory");
            return 0;
        }
        memcpy(c->in + c->in_len, buf, (size_t)n);
        c->in_len += (size_t)n;

        size_t start = 0;
        for (size_t i = 0; i < c->in_len; i++) {
            if (c->in[i] != '\n') {
                continue;
            }
            if (i - start > (size_t)cfg.max_line) {
                log_msg("WARN", "fd=%d line over %ld bytes, closing", c->fd,
                        cfg.max_line);
                static const char too_long[] = "line too long\n";
                send(c->fd, too_long, sizeof too_long - 1, MSG_NOSIGNAL);
                conn_close(c, "line too long");
                return 0;
            }
            if (process_line(c, c->in + start, i - start) == 0) {
                return 0;
            }
            start = i + 1;
        }
        if (start > 0) {
            memmove(c->in, c->in + start, c->in_len - start);
            c->in_len -= start;
        }
        if (c->in_len > (size_t)cfg.max_line) {
            log_msg("WARN", "fd=%d line over %ld bytes, closing", c->fd,
                    cfg.max_line);
            static const char too_long[] = "line too long\n";
            send(c->fd, too_long, sizeof too_long - 1, MSG_NOSIGNAL);
            conn_close(c, "line too long");
            return 0;
        }
        if (c->close_after_flush) {
            break;
        }
    }
    conn_update_events(c);
    return 1;
}

static void conn_new(int fd) {
    struct conn *c;
    struct epoll_event ev;

    if (conn_count == conn_cap) {
        size_t want = conn_cap == 0 ? 16 : conn_cap * 2;
        struct conn **grown = realloc(conns, want * sizeof *grown);
        if (grown == NULL) {
            close(fd);
            return;
        }
        conns = grown;
        conn_cap = want;
    }
    c = calloc(1, sizeof *c);
    if (c == NULL) {
        close(fd);
        return;
    }
    c->fd = fd;
    c->last_ms = now_ms();
    conns[conn_count++] = c;
    memset(&ev, 0, sizeof ev);
    ev.events = EPOLLIN;
    ev.data.ptr = c;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        log_msg("ERROR", "epoll_ctl ADD fd=%d: %s", fd, strerror(errno));
        conn_close(c, "epoll_ctl");
        return;
    }
    if (conn_queue(c, cfg.banner, strlen(cfg.banner)) == 0) {
        conn_queue(c, "\n", 1);
    }
    conn_update_events(c);
    log_msg("INFO", "accept fd=%d conns=%zu", fd, conn_count);
}

static void handle_accept(void) {
    for (;;) {
        int fd = accept4(listener, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            if (errno == EMFILE || errno == ENFILE) {
                /* 预留描述符技巧：腾出一位，接受并立刻拒绝这个连接，
                 * 让监听队列继续前进。 */
                log_msg("ERROR", "accept: %s", strerror(errno));
                if (reserve_fd >= 0) {
                    int extra;
                    static const char busy[] = "server busy\n";
                    close(reserve_fd);
                    extra = accept(listener, NULL, NULL);
                    if (extra >= 0) {
                        send(extra, busy, sizeof busy - 1, MSG_NOSIGNAL);
                        close(extra);
                    }
                    reserve_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
                }
                return;
            }
            log_msg("ERROR", "accept: %s", strerror(errno));
            return;
        }
        if ((long)conn_count >= cfg.max_conn) {
            static const char busy[] = "server busy, try later\n";
            send(fd, busy, sizeof busy - 1, MSG_NOSIGNAL);
            close(fd);
            log_msg("WARN", "reject fd=%d: connection limit %ld", fd,
                    cfg.max_conn);
            continue;
        }
        conn_new(fd);
    }
}

static void handle_signal(void) {
    struct signalfd_siginfo si;

    for (;;) {
        ssize_t n = read(sig_fd, &si, sizeof si);
        if (n != (ssize_t)sizeof si) {
            return;
        }
        if (si.ssi_signo == SIGHUP) {
            if (config_path == NULL) {
                log_msg("WARN", "SIGHUP ignored: no config file");
                continue;
            }
            log_msg("INFO", "SIGHUP: reload %s", config_path);
            if (load_config(config_path, &cfg) == 0) {
                log_msg("INFO",
                        "config reloaded: max_conn=%ld max_line=%ld "
                        "max_out=%ld idle_ms=%ld",
                        cfg.max_conn, cfg.max_line, cfg.max_out, cfg.idle_ms);
            } else {
                log_msg("ERROR", "config rejected, previous one kept");
            }
            continue;
        }
        if (draining) {
            log_msg("WARN", "second signal, exiting immediately");
            exit(EXIT_SUCCESS);
        }
        draining = 1;
        drain_deadline_ms = now_ms() + GRACE_MS;
        log_msg("INFO", "shutdown: stop accepting, flush %zu connections",
                conn_count);
        epoll_ctl(epfd, EPOLL_CTL_DEL, listener, NULL);
        close(listener);
        listener = -1;
        for (size_t i = conn_count; i-- > 0;) {
            struct conn *c = conns[i];
            c->close_after_flush = 1;
            if (c->out_len == 0) {
                conn_close(c, "shutdown");
            } else {
                conn_set_events(c, EPOLLOUT);
            }
        }
    }
}

static void scan_idle(void) {
    int64_t now = now_ms();

    for (size_t i = conn_count; i-- > 0;) {
        struct conn *c = conns[i];
        if (now - c->last_ms >= cfg.idle_ms) {
            log_msg("INFO", "fd=%d idle %ld ms", c->fd, cfg.idle_ms);
            conn_close(c, "idle timeout");
        }
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in addr;
    struct epoll_event ev;
    struct epoll_event events[16];
    sigset_t mask;
    struct sigaction sa;
    long port = DEFAULT_PORT;
    int one = 1;
    int64_t last_scan = 0;

    if (argc > 1 && (parse_long(argv[1], &port) == -1 || port < 1024 ||
                     port > 65535)) {
        fprintf(stderr, "usage: %s [port] [config]\n", argv[0]);
        return EXIT_FAILURE;
    }
    config_path = argc > 2 ? argv[2] : NULL;
    if (config_path != NULL && load_config(config_path, &cfg) == -1) {
        log_msg("ERROR", "cannot load %s", config_path);
        return EXIT_FAILURE;
    }

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    sig_fd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (sig_fd == -1) {
        log_msg("ERROR", "signalfd: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (listener == -1) {
        log_msg("ERROR", "socket: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (struct sockaddr *)&addr, sizeof addr) == -1) {
        log_msg("ERROR", "bind: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    if (listen(listener, 64) == -1) {
        log_msg("ERROR", "listen: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    reserve_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);

    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        log_msg("ERROR", "epoll_create1: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof ev);
    ev.events = EPOLLIN;
    ev.data.ptr = &listener_tag;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev) == -1) {
        log_msg("ERROR", "epoll_ctl: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    memset(&ev, 0, sizeof ev);
    ev.events = EPOLLIN;
    ev.data.ptr = &signal_tag;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sig_fd, &ev) == -1) {
        log_msg("ERROR", "epoll_ctl: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    log_msg("INFO",
            "listening on 127.0.0.1:%ld (max_conn=%ld max_line=%ld "
            "max_out=%ld idle_ms=%ld)",
            port, cfg.max_conn, cfg.max_line, cfg.max_out, cfg.idle_ms);

    for (;;) {
        int n = epoll_wait(epfd, events, 16, LOOP_TICK_MS);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            log_msg("ERROR", "epoll_wait: %s", strerror(errno));
            break;
        }
        for (int i = 0; i < n; i++) {
            void *tag = events[i].data.ptr;
            if (tag == &listener_tag) {
                handle_accept();
            } else if (tag == &signal_tag) {
                handle_signal();
            } else {
                struct conn *c = tag;
                if ((events[i].events & (EPOLLERR | EPOLLHUP)) != 0) {
                    conn_close(c, "socket error");
                    continue;
                }
                if ((events[i].events & EPOLLOUT) != 0 && conn_write(c) == 0) {
                    continue;
                }
                if ((events[i].events & EPOLLIN) != 0 && !draining &&
                    conn_read(c) == 0) {
                    continue;
                }
            }
        }
        if (now_ms() - last_scan >= LOOP_TICK_MS) {
            scan_idle();
            last_scan = now_ms();
        }
        if (draining) {
            if (conn_count == 0) {
                log_msg("INFO", "all connections finished, exiting");
                break;
            }
            if (now_ms() >= drain_deadline_ms) {
                log_msg("WARN", "grace period over, %zu dropped", conn_count);
                break;
            }
        }
    }
    close(epfd);
    if (listener >= 0) {
        close(listener);
    }
    close(sig_fd);
    if (reserve_fd >= 0) {
        close(reserve_fd);
    }
    return EXIT_SUCCESS;
}