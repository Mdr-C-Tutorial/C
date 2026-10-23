#define _POSIX_C_SOURCE 200809L

#include "connection.h"

#include "http.h"
#include "server.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// 连接的状态机
//
//   accept --> READING --收齐一个请求头--> WRITING --发完，保持连接--> READING
//                 |                          |
//                 | 对方关闭或出错           | 发完，不保持连接
//                 v                          v
//               CLOSED <-----对方关闭----- LINGERING
//
// 此外，WRITING 中写出错直接转入 CLOSED；任何状态下超时都直接关闭
// （由 server.c 处理）。
//
// 每个状态只在 epoll 中登记一种事件：READING 和 LINGERING 登记可读，
// WRITING 登记可写。水平触发模式下，只要条件成立 epoll_wait 就一直报告；
// 没有数据要发却登记着可写，事件循环就会空转。所以登记的事件必须随状态
// 切换（见 set_events）。
//
// 出错或对方关闭时，处理函数只把状态设为 CONN_CLOSED 然后返回，由事件
// 循环在处理函数返回之后统一释放连接。处理函数执行到一半时连接不会被
// 释放，调用栈上层的代码也就不会用到已经释放的连接。

// 一次 send_response 最多调用几次 write（每次至多一个输出缓冲区）。读得
// 快的客户端下载大文件时，write 可能一直不返回 EAGAIN；写满这个份额就
// 回到事件循环，其他连接才不会被饿着。输入缓冲区里还有流水线发来的请求
// 时，同一个事件会接着回应它们，每个响应各有一份，总量受输入缓冲区的
// 大小限制。
#define MAX_WRITES_PER_EVENT 4

static void process_input(struct connection *conn);
static void send_response(struct connection *conn);

static bool is_temporary_error(int err) {
    return err == EAGAIN || err == EWOULDBLOCK || err == EINTR;
}

// 读到或写出了数据，说明连接有进展，重新计算空闲时间。
static void touch(struct connection *conn) {
    struct server *srv = conn->server;
    timer_restart(&srv->idle_timers, &conn->idle_timer, srv->now_ms);
}

// 修改连接在 epoll 中登记的事件；和当前相同时省掉这次系统调用。
static void set_events(struct connection *conn, uint32_t events) {
    if (conn->events == events) {
        return;
    }
    struct epoll_event ev = {.events = events, .data.ptr = conn};
    if (epoll_ctl(conn->server->epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev) < 0) {
        conn->state = CONN_CLOSED;
        return;
    }
    conn->events = events;
}

bool connection_slot_init(struct connection *conn, size_t input_size) {
    memset(conn, 0, sizeof *conn);
    conn->state = CONN_FREE;
    conn->fd = -1;
    conn->file_fd = -1;
    timer_init(&conn->idle_timer, conn);
    return buffer_init(&conn->in, input_size) &&
           buffer_init(&conn->out, CONN_OUTPUT_BUFFER_SIZE);
}

void connection_slot_free(struct connection *conn) {
    buffer_free(&conn->in);
    buffer_free(&conn->out);
}

bool connection_open(struct connection *conn, struct server *srv, int fd,
                     const char *peer) {
    conn->server = srv;
    conn->fd = fd;
    conn->state = CONN_READING;
    conn->events = EPOLLIN;
    conn->file_fd = -1;
    conn->file_remaining = 0;
    conn->keep_alive = false;
    buffer_clear(&conn->in);
    buffer_clear(&conn->out);
    snprintf(conn->peer, sizeof conn->peer, "%s", peer);

    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = conn};
    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return false;
    }
    touch(conn);
    return true;
}

bool connection_is_idle(const struct connection *conn) {
    return conn->state == CONN_READING && buffer_length(&conn->in) == 0;
}

void connection_close(struct connection *conn) {
    timer_cancel(&conn->idle_timer);
    if (conn->file_fd >= 0) {
        close(conn->file_fd);
        conn->file_fd = -1;
    }
    // close 会把描述符从 epoll 的兴趣列表中删掉，不必先 EPOLL_CTL_DEL。
    // 前提是没有别的描述符指向同一个打开的套接字，本程序从不 dup 或 fork。
    close(conn->fd);
    conn->fd = -1;
    conn->state = CONN_FREE;
}

// ---------------------------------------------------------------------------
// 准备响应

static void log_access(const struct connection *conn,
                       const struct http_request *req, int status,
                       uint64_t length) {
    server_log(conn->server, LOG_INFO, "%s \"%s %s\" %d %" PRIu64, conn->peer,
               req->method[0] != '\0' ? req->method : "-",
               req->target[0] != '\0' ? req->target : "-", status, length);
}

// 把响应头写进输出缓冲区。准备响应时输出缓冲区总是空的，而响应头只有
// 几百字节，所以实际上不会失败。
static bool append_head(struct connection *conn,
                        const struct http_response *resp) {
    size_t n = http_format_head(buffer_tail(&conn->out),
                                buffer_space(&conn->out), resp);
    if (n == 0) {
        return false;
    }
    buffer_commit(&conn->out, n);
    return true;
}

// 准备只带一行文字说明的响应，用于错误和重定向。
static void prepare_simple_response(struct connection *conn,
                                    const struct http_request *req, int status,
                                    const char *location) {
    char body[64];
    int body_length = snprintf(body, sizeof body, "%d %s\n", status,
                               http_reason_phrase(status));
    struct http_response resp = {
        .status = status,
        .content_type = "text/plain; charset=utf-8",
        .content_length = (uint64_t)body_length,
        .location = location,
        .keep_alive = conn->keep_alive,
        .date = time(NULL),
    };
    // HEAD 的响应只有响应头，Content-Length 照常给出 GET 时正文的长度。
    bool send_body = strcmp(req->method, "HEAD") != 0;
    if (!append_head(conn, &resp) ||
        (send_body && !buffer_append(&conn->out, body, (size_t)body_length))) {
        conn->state = CONN_CLOSED;
        return;
    }
    log_access(conn, req, status, resp.content_length);
    conn->state = CONN_WRITING;
}

// 请求目标中查询串之前的部分有多长。
static size_t path_part_length(const char *target) {
    return strcspn(target, "?");
}

// 目录的 URL 应以 '/' 结尾，否则页面里的相对链接会相对上一级目录解析。
// 和多数静态文件服务器一样，回 301 让浏览器改用带 '/' 的地址，查询串
// 照原样带上。开头多余的 '/' 要去掉：浏览器把 "//docs/" 当成省略了协议
// 的网址，会跳到主机名为 docs 的另一个网站，这就成了开放重定向。
static void prepare_redirect(struct connection *conn,
                             const struct http_request *req) {
    const char *path = req->target;
    while (path[1] == '/') {
        path++;
    }
    size_t path_length = path_part_length(path);
    char location[HTTP_MAX_TARGET + 2];
    snprintf(location, sizeof location, "%.*s/%s", (int)path_length, path,
             path + path_length);
    prepare_simple_response(conn, req, 301, location);
}

// 处理一个解析成功的请求：找到文件，准备响应头，记下要发送的文件。
static void handle_request(struct connection *conn,
                           const struct http_request *req) {
    struct server *srv = conn->server;
    // 本服务器不读请求体。请求带有请求体时，回应之后关闭连接，否则
    // 请求体会被当成下一个请求的开头来解析。
    conn->keep_alive = req->keep_alive && !req->has_body && !srv->shutting_down;

    bool is_head = strcmp(req->method, "HEAD") == 0;
    if (!is_head && strcmp(req->method, "GET") != 0) {
        prepare_simple_response(conn, req, 501, NULL);
        return;
    }

    char path[HTTP_MAX_TARGET + sizeof "/index.html"];
    int status = http_target_to_path(req->target, path, sizeof path);
    if (status != 0) {
        prepare_simple_response(conn, req, status, NULL);
        return;
    }

    // path 是相对路径，openat 相对文档根目录去找它。加 O_NONBLOCK 是为了
    // 防止路径指向 FIFO 时 open 一直等写端出现，卡住整个事件循环。
    int fd = openat(srv->root_fd, path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
        status = errno == ENOENT || errno == ENOTDIR ? 404
                 : errno == EACCES                   ? 403
                                                     : 500;
        prepare_simple_response(conn, req, status, NULL);
        return;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        prepare_simple_response(conn, req, 500, NULL);
        return;
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        size_t path_length = path_part_length(req->target);
        if (S_ISDIR(st.st_mode) && req->target[path_length - 1] != '/') {
            prepare_redirect(conn, req);
        } else {
            prepare_simple_response(conn, req, 404, NULL);
        }
        return;
    }

    struct http_response resp = {
        .status = 200,
        .content_type = http_content_type(path),
        .content_length = (uint64_t)st.st_size,
        .keep_alive = conn->keep_alive,
        .date = time(NULL),
    };
    if (!append_head(conn, &resp)) {
        close(fd);
        conn->state = CONN_CLOSED;
        return;
    }
    // 正文不在这里读：文件可能很大。send_response 每次从文件读一个输出
    // 缓冲区的量，写出去以后再读下一段。
    if (!is_head && st.st_size > 0) {
        conn->file_fd = fd;
        conn->file_remaining = (uint64_t)st.st_size;
    } else {
        close(fd);
    }
    log_access(conn, req, 200, resp.content_length);
    conn->state = CONN_WRITING;
}

// 从输入缓冲区取出完整的请求，逐个处理。一次 read 可能只收到半个请求头，
// 也可能收到好几个请求（HTTP 流水线）；后一种情况按收到的顺序依次回应
// （RFC 9112 §9.3.2）。
static void process_input(struct connection *conn) {
    while (conn->state == CONN_READING && buffer_length(&conn->in) > 0) {
        struct http_request req;
        enum http_parse_result result = http_parse_request(
            buffer_data(&conn->in), buffer_length(&conn->in), &req);
        if (result == HTTP_PARSE_INCOMPLETE && !buffer_is_full(&conn->in)) {
            return; // 请求头还没收齐，等下一次可读
        }
        if (result == HTTP_PARSE_OK) {
            buffer_consume(&conn->in, req.head_length);
            handle_request(conn, &req);
        } else {
            // 请求有误，或者缓冲区满了请求头还没完。后面的数据无从解析，
            // 也就找不到下一个请求从哪里开始，回应之后关闭连接。
            conn->keep_alive = false;
            int status = result == HTTP_PARSE_ERROR ? req.error_status : 431;
            prepare_simple_response(conn, &req, status, NULL);
        }
        // 准备响应时出了错，状态已经是 CONN_CLOSED，这时不能再发送：
        // finish_response 会把它改回 CONN_READING 或 CONN_LINGERING。
        if (conn->state == CONN_WRITING) {
            send_response(conn);
        }
    }
}

// ---------------------------------------------------------------------------
// 发送响应

// 把文件的下一段读进输出缓冲区，缓冲区腾出多少空间就读多少。
static bool fill_from_file(struct connection *conn) {
    buffer_compact(&conn->out);
    size_t want = buffer_space(&conn->out);
    if (want > conn->file_remaining) {
        want = (size_t)conn->file_remaining;
    }
    if (want == 0) {
        return true;
    }
    // 普通文件的 read 从不返回 EAGAIN：数据不在页缓存里时它会阻塞，
    // 整个事件循环跟着等磁盘。这是单线程服务器的已知代价，见 README。
    ssize_t n = read(conn->file_fd, buffer_tail(&conn->out), want);
    if (n <= 0) {
        // 文件在发送过程中被截短了。Content-Length 已经发出去，
        // 只能断开连接，让客户端知道正文不完整。
        return false;
    }
    buffer_commit(&conn->out, (size_t)n);
    conn->file_remaining -= (uint64_t)n;
    if (conn->file_remaining == 0) {
        close(conn->file_fd);
        conn->file_fd = -1;
    }
    return true;
}

static void finish_response(struct connection *conn) {
    if (conn->keep_alive) {
        conn->state = CONN_READING;
        set_events(conn, EPOLLIN);
        return;
    }
    // 分阶段关闭（RFC 9112 §9.6）：先关闭写方向，对方收到 FIN 就知道
    // 响应结束了；然后继续读，直到对方也关闭。如果直接 close，而接收缓冲区
    // 里还有对方发来、我们没读的数据，内核会发 RST 而不是 FIN：对方若还在
    // 发送，send 会失败；已经送到的响应，对方也可能来不及读。
    shutdown(conn->fd, SHUT_WR);
    conn->state = CONN_LINGERING;
    set_events(conn, EPOLLIN);
}

static void send_response(struct connection *conn) {
    for (int i = 0; i < MAX_WRITES_PER_EVENT; i++) {
        if (conn->file_remaining > 0 && !fill_from_file(conn)) {
            conn->state = CONN_CLOSED;
            return;
        }
        if (buffer_length(&conn->out) == 0) {
            finish_response(conn);
            return;
        }
        ssize_t n =
            write(conn->fd, buffer_data(&conn->out), buffer_length(&conn->out));
        if (n < 0) {
            if (!is_temporary_error(errno)) {
                conn->state = CONN_CLOSED;
                return;
            }
            break; // 套接字的发送缓冲区满了
        }
        buffer_consume(&conn->out, (size_t)n);
        touch(conn);
    }
    // 还有数据没发完：等套接字重新可写时接着发。
    set_events(conn, EPOLLOUT);
}

// ---------------------------------------------------------------------------
// 事件处理

static void on_readable(struct connection *conn) {
    // 输入缓冲区满了还没收齐请求头时，process_input 已经回了 431、离开了
    // CONN_READING，所以整理之后这里总有空间。
    buffer_compact(&conn->in);
    ssize_t n = read(conn->fd, buffer_tail(&conn->in), buffer_space(&conn->in));
    if (n > 0) {
        buffer_commit(&conn->in, (size_t)n);
        touch(conn);
        process_input(conn);
    } else if (n == 0 || !is_temporary_error(errno)) {
        conn->state = CONN_CLOSED; // 对方关闭了连接，或者出错
    }
}

static void on_writable(struct connection *conn) {
    send_response(conn);
    // 响应发完回到 CONN_READING 时，输入缓冲区里可能已经有流水线发来的
    // 下一个请求，不必等新的可读事件。
    if (conn->state == CONN_READING) {
        process_input(conn);
    }
}

static void on_lingering(struct connection *conn) {
    char discard[16384];
    ssize_t n = read(conn->fd, discard, sizeof discard);
    // 读到的数据直接扔掉。这里不重置定时器：对方一直发数据，最多也只再
    // 等一个空闲超时。
    if (n == 0 || (n < 0 && !is_temporary_error(errno))) {
        conn->state = CONN_CLOSED;
    }
}

void connection_handle_event(struct connection *conn) {
    // 不区分是哪种事件。连接只登记了当前状态需要的那一种；EPOLLERR 和
    // EPOLLHUP 不用登记也会报告，这时接下来的 read 或 write 会返回 0
    // 或错误码，处理函数照常据此关闭连接。
    switch (conn->state) {
    case CONN_READING:
        on_readable(conn);
        break;
    case CONN_WRITING:
        on_writable(conn);
        break;
    case CONN_LINGERING:
        on_lingering(conn);
        break;
    case CONN_FREE:
    case CONN_CLOSED:
        break;
    }
}
