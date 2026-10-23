// accept4 是 Linux 的扩展（POSIX.1-2024 才收入标准），glibc 要求定义
// _GNU_SOURCE 才会声明它。
#define _GNU_SOURCE

#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// 一次 epoll_wait 最多取回的事件数。没取完的下一轮再取，不会丢失。
#define MAX_EVENTS 64

// 每个连接最多同时占两个描述符：套接字和正在发送的文件。另外预留一些
// 给标准输入输出、监听套接字、epoll、signalfd 和文档根目录。
#define RESERVED_FDS 16

void server_log(const struct server *srv, enum log_level level,
                const char *format, ...) {
    if (level > srv->config.log_level) {
        return;
    }
    char line[1024];
    size_t n = 0;
    time_t now = time(NULL);
    struct tm tm;
    if (localtime_r(&now, &tm) != NULL) {
        n = strftime(line, sizeof line, "%H:%M:%S ", &tm);
    }
    va_list args;
    va_start(args, format);
    vsnprintf(line + n, sizeof line - n, format, args);
    va_end(args);
    fprintf(stderr, "%s\n", line);
}

// 单调时钟不受系统时间调整的影响，计算超时只能用它。
static int64_t monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void close_fd(int *fd) {
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

// 把 fd 登记到 epoll，关注可读事件。tag 会原样出现在事件的 data.ptr 中，
// 事件循环靠它分辨事件来自哪里。
static int watch_readable(struct server *srv, int fd, void *tag) {
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = tag};
    return epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

// ---------------------------------------------------------------------------
// 接受连接

static void start_accepting(struct server *srv) {
    if (srv->accepting || srv->listen_fd < 0) {
        return;
    }
    if (watch_readable(srv, srv->listen_fd, &srv->listen_fd) < 0) {
        server_log(srv, LOG_ERROR, "epoll_ctl(listen): %s", strerror(errno));
        return;
    }
    srv->accepting = true;
}

// 暂停接受新连接。接受队列里有连接时监听套接字一直可读，水平触发模式下
// epoll_wait 会不停地报告它；连接池满了（或描述符用完了）还留着它，事件
// 循环就会空转。所以要把它移出 epoll，等有连接关闭再放回去。这期间新来
// 的客户端由内核挂在接受队列里。
static void stop_accepting(struct server *srv) {
    if (!srv->accepting) {
        return;
    }
    epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, srv->listen_fd, NULL);
    srv->accepting = false;
}

static void release_connection(struct server *srv, struct connection *conn) {
    server_log(srv, LOG_DEBUG, "%s closed", conn->peer);
    connection_close(conn);
    conn->next_free = srv->free_list;
    srv->free_list = conn;
    srv->active_connections--;
    if (!srv->shutting_down) {
        start_accepting(srv);
    }
}

static void format_peer(const struct sockaddr_in *addr, char *out,
                        size_t size) {
    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof ip) == NULL) {
        snprintf(out, size, "?");
        return;
    }
    snprintf(out, size, "%s:%u", ip, (unsigned)ntohs(addr->sin_port));
}

static void accept_connections(struct server *srv) {
    while (srv->free_list != NULL) {
        struct sockaddr_in addr;
        socklen_t addr_length = sizeof addr;
        // 新套接字直接设成非阻塞和 close-on-exec，省掉两次 fcntl。
        int fd = accept4(srv->listen_fd, (struct sockaddr *)&addr, &addr_length,
                         SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EINTR || errno == ECONNABORTED) {
                continue;
            }
            if (errno == EMFILE || errno == ENFILE) {
                server_log(srv, LOG_ERROR,
                           "accept: %s; paused until a connection closes",
                           strerror(errno));
                stop_accepting(srv);
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                server_log(srv, LOG_ERROR, "accept: %s", strerror(errno));
            }
            return;
        }

        // 关掉 Nagle 算法。它会把小块数据攒到前面的数据被确认之后再发，
        // 流水线上连续的几个小响应可能因此各多等几十毫秒。
        int on = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);

        char peer[32];
        format_peer(&addr, peer, sizeof peer);
        struct connection *conn = srv->free_list;
        srv->free_list = conn->next_free;
        srv->active_connections++;
        if (!connection_open(conn, srv, fd, peer)) {
            server_log(srv, LOG_ERROR, "epoll_ctl(%s): %s", peer,
                       strerror(errno));
            release_connection(srv, conn);
            continue;
        }
        server_log(srv, LOG_DEBUG, "%s connected (%d active)", peer,
                   srv->active_connections);
    }
    server_log(srv, LOG_DEBUG, "connection limit %d reached, accept paused",
               srv->config.max_connections);
    stop_accepting(srv);
}

// ---------------------------------------------------------------------------
// 信号与退出

static void read_signals(struct server *srv) {
    struct signalfd_siginfo info;
    while (read(srv->signal_fd, &info, sizeof info) == sizeof info) {
        const char *name = info.ssi_signo == SIGINT ? "SIGINT" : "SIGTERM";
        if (srv->shutdown_requested) {
            server_log(srv, LOG_INFO, "received %s again, exiting now", name);
            srv->stop = true;
        } else {
            server_log(srv, LOG_INFO, "received %s, shutting down", name);
            srv->shutdown_requested = true;
        }
    }
}

// 优雅退出：不再接受新连接；空闲的长连接直接关闭；正在处理的请求做完
// 再关闭。已经关闭监听套接字的服务器，在最后一个连接关闭后退出事件循环。
static void begin_shutdown(struct server *srv) {
    srv->shutting_down = true;
    stop_accepting(srv);
    close_fd(&srv->listen_fd);
    for (int i = 0; i < srv->config.max_connections; i++) {
        struct connection *conn = &srv->connections[i];
        if (conn->state == CONN_FREE) {
            continue;
        }
        if (connection_is_idle(conn)) {
            release_connection(srv, conn);
        } else {
            conn->keep_alive = false;
        }
    }
    server_log(srv, LOG_INFO, "waiting for %d connection(s) to finish",
               srv->active_connections);
}

static void close_idle_connections(struct server *srv) {
    struct timer *timer;
    while ((timer = timer_queue_pop_expired(&srv->idle_timers, srv->now_ms)) !=
           NULL) {
        struct connection *conn = timer->owner;
        server_log(srv, LOG_DEBUG, "%s idle timeout", conn->peer);
        release_connection(srv, conn);
    }
}

// ---------------------------------------------------------------------------
// 事件循环

static void dispatch(struct server *srv, void *tag) {
    if (tag == &srv->listen_fd) {
        accept_connections(srv);
    } else if (tag == &srv->signal_fd) {
        read_signals(srv);
    } else {
        struct connection *conn = tag;
        connection_handle_event(conn);
        if (conn->state == CONN_CLOSED) {
            release_connection(srv, conn);
        }
    }
}

int server_run(struct server *srv) {
    const struct server_config *cfg = &srv->config;
    size_t buffer_bytes = cfg->max_request_size + CONN_OUTPUT_BUFFER_SIZE;
    server_log(srv, LOG_INFO, "serving %s on http://%s:%d/", cfg->root,
               cfg->address, cfg->port);
    server_log(srv, LOG_INFO,
               "limits: %d connections, %zu-byte request heads, "
               "%d ms idle timeout, %zu KiB of buffers per connection",
               cfg->max_connections, cfg->max_request_size,
               cfg->idle_timeout_ms, buffer_bytes / 1024);

    struct epoll_event events[MAX_EVENTS];
    while (!srv->stop) {
        int timeout = timer_queue_wait_ms(&srv->idle_timers, monotonic_ms());
        int n = epoll_wait(srv->epoll_fd, events, MAX_EVENTS, timeout);
        if (n < 0) {
            if (errno == EINTR) {
                continue; // 例如进程被 SIGSTOP 暂停后又继续
            }
            server_log(srv, LOG_ERROR, "epoll_wait: %s", strerror(errno));
            return -1;
        }
        // 每轮只读一次时钟，这一轮里的处理函数都用这个时间。
        srv->now_ms = monotonic_ms();
        for (int i = 0; i < n; i++) {
            dispatch(srv, events[i].data.ptr);
        }
        // 下面这些操作会关闭“别的”连接，所以放在处理完这一批事件之后。
        // 如果在批次中间关闭并回收了一个连接，同一批里还没处理的事件可能
        // 正指向它。
        close_idle_connections(srv);
        if (srv->shutdown_requested && !srv->shutting_down) {
            begin_shutdown(srv);
        }
        if (srv->shutting_down && srv->active_connections == 0) {
            srv->stop = true;
        }
    }
    server_log(srv, LOG_INFO, "bye");
    return 0;
}

// ---------------------------------------------------------------------------
// 初始化与清理

// 连接数上限受进程的描述符上限（RLIMIT_NOFILE）约束。软限制不够时先把它
// 提高到硬限制（普通用户也可以这样做），还不够就降低连接数上限。这样
// 资源不足在启动时就暴露出来，而不是等到运行中 accept 或 open 失败。
static void fit_fd_limit(struct server *srv) {
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) < 0) {
        return;
    }
    rlim_t needed = (rlim_t)srv->config.max_connections * 2 + RESERVED_FDS;
    if (limit.rlim_cur >= needed) {
        return;
    }
    rlim_t old = limit.rlim_cur;
    limit.rlim_cur = needed < limit.rlim_max ? needed : limit.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &limit) < 0) {
        limit.rlim_cur = old;
    }
    if (limit.rlim_cur != old) {
        server_log(srv, LOG_INFO, "descriptor limit raised from %llu to %llu",
                   (unsigned long long)old, (unsigned long long)limit.rlim_cur);
    }
    if (limit.rlim_cur < needed) {
        int fit = limit.rlim_cur > RESERVED_FDS + 2
                      ? (int)((limit.rlim_cur - RESERVED_FDS) / 2)
                      : 1;
        server_log(srv, LOG_INFO,
                   "descriptor limit is %llu, max connections lowered to %d",
                   (unsigned long long)limit.rlim_cur, fit);
        srv->config.max_connections = fit;
    }
}

// 连接池：启动时一次分配全部槽位和缓冲区，运行中不再分配内存。连接数的
// 上限就是槽位的个数。
static int create_pool(struct server *srv) {
    int count = srv->config.max_connections;
    srv->connections = calloc((size_t)count, sizeof *srv->connections);
    if (srv->connections == NULL) {
        return -1;
    }
    // 倒着串成空闲链表，槽位就从 0 号开始分配，调试时更直观。
    for (int i = count - 1; i >= 0; i--) {
        struct connection *conn = &srv->connections[i];
        if (!connection_slot_init(conn, srv->config.max_request_size)) {
            return -1;
        }
        conn->next_free = srv->free_list;
        srv->free_list = conn;
    }
    return 0;
}

// 把 SIGINT、SIGTERM 变成 signalfd 上的可读事件，和网络事件在同一个循环
// 里处理。两个信号必须先阻塞，否则它们仍按默认动作直接终止进程，根本
// 不会出现在 signalfd 上。
static int setup_signals(struct server *srv) {
    // 向对方已经关闭的连接写数据时内核会发 SIGPIPE，默认动作是终止进程。
    // 忽略它之后 write 返回 EPIPE，当作普通错误处理。
    signal(SIGPIPE, SIG_IGN);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        return -1;
    }
    srv->signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (srv->signal_fd < 0) {
        return -1;
    }
    return watch_readable(srv, srv->signal_fd, &srv->signal_fd);
}

static int open_listener(struct server *srv) {
    const struct server_config *cfg = &srv->config;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)cfg->port);
    if (inet_pton(AF_INET, cfg->address, &addr.sin_addr) != 1) {
        server_log(srv, LOG_ERROR, "not an IPv4 address: %s", cfg->address);
        return -1;
    }

    srv->listen_fd =
        socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (srv->listen_fd < 0) {
        server_log(srv, LOG_ERROR, "socket: %s", strerror(errno));
        return -1;
    }
    // 端口上还有 TIME_WAIT 状态的旧连接时也允许绑定，服务器重启时不必
    // 等上一两分钟。它不允许两个进程同时监听同一个端口。
    int on = 1;
    setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    if (bind(srv->listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        server_log(srv, LOG_ERROR, "bind %s:%d: %s", cfg->address, cfg->port,
                   strerror(errno));
        return -1;
    }
    if (listen(srv->listen_fd, SOMAXCONN) < 0) {
        server_log(srv, LOG_ERROR, "listen: %s", strerror(errno));
        return -1;
    }
    start_accepting(srv);
    return srv->accepting ? 0 : -1;
}

int server_init(struct server *srv, const struct server_config *config) {
    memset(srv, 0, sizeof *srv);
    srv->config = *config;
    srv->listen_fd = -1;
    srv->epoll_fd = -1;
    srv->signal_fd = -1;
    srv->root_fd = -1;
    srv->now_ms = monotonic_ms();
    timer_queue_init(&srv->idle_timers, config->idle_timeout_ms);

    fit_fd_limit(srv);
    if (create_pool(srv) < 0) {
        server_log(srv, LOG_ERROR, "out of memory for %d connections",
                   srv->config.max_connections);
        return -1;
    }
    srv->root_fd = open(config->root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (srv->root_fd < 0) {
        server_log(srv, LOG_ERROR, "document root %s: %s", config->root,
                   strerror(errno));
        return -1;
    }
    srv->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (srv->epoll_fd < 0) {
        server_log(srv, LOG_ERROR, "epoll_create1: %s", strerror(errno));
        return -1;
    }
    if (setup_signals(srv) < 0) {
        server_log(srv, LOG_ERROR, "signals: %s", strerror(errno));
        return -1;
    }
    return open_listener(srv);
}

void server_cleanup(struct server *srv) {
    if (srv->connections != NULL) {
        for (int i = 0; i < srv->config.max_connections; i++) {
            struct connection *conn = &srv->connections[i];
            if (conn->state != CONN_FREE) {
                connection_close(conn);
            }
            connection_slot_free(conn);
        }
        free(srv->connections);
        srv->connections = NULL;
    }
    close_fd(&srv->listen_fd);
    close_fd(&srv->signal_fd);
    close_fd(&srv->epoll_fd);
    close_fd(&srv->root_fd);
}
