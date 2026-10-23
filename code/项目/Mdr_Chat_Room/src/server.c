#define _POSIX_C_SOURCE 200809L

#include "net.h"
#include "protocol.h"
#include "room.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * 每个连接的发送缓冲区。对方不读数据时，先填满内核里的发送缓冲区，然后堆积在
 * 这里；这里也满了就断开它。单线程的服务器不能停下来等一个慢客户端，也不能无限
 * 制地替它攒数据。
 */
#define OUTBUF_SIZE (16 * 1024)

typedef struct {
    int fd;                         /* -1 表示槽位空闲，此时其余成员没有意义 */
    bool closing;                   /* 已决定断开，下一轮循环开头再真正关闭 */
    frame_buffer in;                /* 收到了、还没凑成整帧的字节 */
    unsigned char out[OUTBUF_SIZE]; /* 已经编码、内核还没收下的字节 */
    size_t out_len;
} connection;

typedef struct {
    int listen_fd;
    bool verbose;
    connection conns[ROOM_CAPACITY]; /* 下标同时就是聊天室里的成员编号 */
    chat_room room;
} chat_server;

static volatile sig_atomic_t stop_requested = 0;

static void on_stop_signal(int sig) {
    (void)sig;
    stop_requested = 1;
}

static void trace_bytes(int id, const unsigned char *bytes, size_t len) {
    fprintf(stderr, "[#%d] read %zu byte(s):", id, len);
    for (size_t i = 0; i < len && i < 32; i++) {
        fprintf(stderr, " %02x", bytes[i]);
    }
    fputs(len > 32 ? " ...\n" : "\n", stderr);
}

static void trace_message(int id, const char *direction, const message *msg) {
    fprintf(stderr, "[#%d] %s %s name=\"%s\" text=\"%s\"\n", id, direction,
            msg_type_name(msg->type), msg->name, msg->text);
}

/* 把发送缓冲区尽量交给内核。内核收不下的部分留在缓冲区里，
 * 主循环看到 out_len > 0 就会关注 POLLOUT，等套接字可写时再来。 */
static void conn_flush(connection *c) {
    size_t sent = 0;
    while (sent < c->out_len) {
        ssize_t n = send(c->fd, c->out + sent, c->out_len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                c->closing = true; /* 对方已经断开（EPIPE、ECONNRESET 等） */
            }
            break;
        }
        sent += (size_t)n;
    }
    memmove(c->out, c->out + sent, c->out_len - sent);
    c->out_len -= sent;
}

/* 聊天室的发送回调：把消息编码进连接的发送缓冲区，然后立即试着发出。 */
static void deliver(void *ctx, int id, const message *msg) {
    chat_server *srv = ctx;
    connection *c = &srv->conns[id];
    if (c->fd < 0 || c->closing) {
        return;
    }
    size_t n =
        proto_encode(msg, c->out + c->out_len, sizeof c->out - c->out_len);
    if (n == 0) {
        fprintf(stderr, "[#%d] output buffer full, dropping client\n", id);
        /* 这里只做标记：此刻可能正在聊天室群发的循环里，立即关闭会调用
         * room_leave，在遍历成员表的途中修改它。 */
        c->closing = true;
        return;
    }
    c->out_len += n;
    if (srv->verbose) {
        trace_message(id, "send", msg);
    }
    conn_flush(c);
}

static void reject_frame(chat_server *srv, int id, proto_status status) {
    fprintf(stderr, "[#%d] protocol error: %s\n", id,
            proto_status_text(status));
    message msg;
    message_set(&msg, MSG_ERROR, NULL, proto_status_text(status));
    deliver(srv, id, &msg);
    /* 长度前缀的字节流一旦解析出错，就无法确定下一帧从哪里开始，
     * 这条连接上的后续数据都没法用了，只能断开。 */
    srv->conns[id].closing = true;
}

static void read_client(chat_server *srv, int id) {
    connection *c = &srv->conns[id];
    frame_buffer *in = &c->in;
    /* 缓冲区满时其中必有一整帧，上一次调用已经把它取走，所以这里总有空间。
     * 若 recv 的长度参数为 0，它会返回 0，被误当成对方关闭了连接。 */
    assert(in->len < sizeof in->data);

    ssize_t n = recv(c->fd, in->data + in->len, sizeof in->data - in->len, 0);
    if (n == 0) {
        c->closing = true; /* 对方关闭了连接，或者半关闭，不会再发数据 */
        return;
    }
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            c->closing = true;
        }
        return;
    }
    if (srv->verbose) {
        trace_bytes(id, in->data + in->len, (size_t)n);
    }
    in->len += (size_t)n;

    /* 一次 recv 拿到的可能是半帧、一帧或好几帧：
     * 取出所有完整的帧，剩下的半帧留在缓冲区里等下一次。 */
    for (;;) {
        message msg;
        proto_status status = frame_buffer_next(in, &msg);
        if (status == PROTO_INCOMPLETE) {
            break;
        }
        if (status != PROTO_OK) {
            reject_frame(srv, id, status);
            break;
        }
        if (srv->verbose) {
            trace_message(id, "recv", &msg);
        }
        room_handle(&srv->room, id, &msg);
        if (c->closing) {
            break; /* 处理途中发现连接已坏（例如发给它自己的回复发不出去） */
        }
    }
}

/* 只用于新连接被拒绝时。新连接的内核发送缓冲区是空的，几十个字节的一帧
 * 一次就能交给内核，所以直接发送，不经过连接表和 POLLOUT。 */
static void reject_connection(int fd, const char *reason) {
    message msg;
    unsigned char frame[PROTO_FRAME_MAX];
    message_set(&msg, MSG_ERROR, NULL, reason);
    size_t n = proto_encode(&msg, frame, sizeof frame);
    (void)net_send_all(fd, frame, n);
    close(fd);
}

static void accept_client(chat_server *srv) {
    int fd = accept(srv->listen_fd, NULL, NULL);
    if (fd < 0) {
        /* ECONNABORTED：对方在我们 accept 之前就断开了，不影响服务器 */
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR &&
            errno != ECONNABORTED) {
            perror("accept");
        }
        return;
    }
    int id = 0;
    while (id < ROOM_CAPACITY && srv->conns[id].fd >= 0) {
        id++;
    }
    if (id == ROOM_CAPACITY) {
        fprintf(stderr, "server is full, rejecting a connection\n");
        reject_connection(fd, "server is full");
        return;
    }
    if (net_set_nonblocking(fd) < 0) {
        perror("fcntl");
        close(fd);
        return;
    }
    connection *c = &srv->conns[id];
    c->fd = fd;
    c->closing = false;
    c->in.len = 0;
    c->out_len = 0;

    char peer[80];
    net_peer_name(fd, peer, sizeof peer);
    fprintf(stderr, "[#%d] connected from %s\n", id, peer);
}

/*
 * 关闭所有被标记的连接。关闭推迟到每轮循环开头统一进行，这样处理事件的途中
 * 连接表不会变化：deliver 发现某个连接坏了，只需做标记，不必担心自己正处在
 * 群发循环里。
 */
static void close_marked(chat_server *srv) {
    bool closed_any = true;
    while (closed_any) {
        closed_any = false;
        for (int id = 0; id < ROOM_CAPACITY; id++) {
            connection *c = &srv->conns[id];
            if (c->fd < 0 || !c->closing) {
                continue;
            }
            close(c->fd);
            c->fd = -1;
            fprintf(stderr, "[#%d] disconnected\n", id);
            /* 群发离开通知时，别的连接可能因为发送缓冲区溢出而被标记，
             * 所以外层循环要再扫一遍，直到没有新的标记。 */
            room_leave(&srv->room, id);
            closed_any = true;
        }
    }
}

static int run(chat_server *srv) {
    struct pollfd fds[1 + ROOM_CAPACITY];
    int ids[1 + ROOM_CAPACITY]; /* fds[k] 属于哪个连接；fds[0] 是监听套接字 */

    while (!stop_requested) {
        close_marked(srv);

        /* 每轮都从连接表重建 fds。最多 33 项，重建的开销可以忽略，
         * 换来的是不必在连接增减时同步维护两张表。 */
        nfds_t count = 0;
        fds[count] = (struct pollfd){.fd = srv->listen_fd, .events = POLLIN};
        ids[count++] = -1;
        for (int id = 0; id < ROOM_CAPACITY; id++) {
            const connection *c = &srv->conns[id];
            if (c->fd < 0) {
                continue;
            }
            /* 只在有数据待发时关注 POLLOUT。套接字几乎总是可写的，
             * 一直关注它，poll 就会立即返回，主循环原地空转。 */
            short events = POLLIN;
            if (c->out_len > 0) {
                events |= POLLOUT;
            }
            fds[count] = (struct pollfd){.fd = c->fd, .events = events};
            ids[count++] = id;
        }

        /* 超时设为 1 秒：信号若恰好在检查 stop_requested 之后、进入 poll
         * 之前到达，poll 不会被它打断；有了超时，最多晚 1 秒退出。 */
        if (poll(fds, count, 1000) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            return -1;
        }

        if (fds[0].revents & POLLIN) {
            accept_client(srv);
        }
        for (nfds_t k = 1; k < count; k++) {
            connection *c = &srv->conns[ids[k]];
            short revents = fds[k].revents;
            if (revents == 0 || c->closing) {
                continue;
            }
            /* 对方关闭或出错时 poll 报告 POLLHUP/POLLERR，
             * 交给 recv，由它的返回值说明是哪种情况。 */
            if (revents & (POLLIN | POLLHUP | POLLERR)) {
                read_client(srv, ids[k]);
            }
            if ((revents & POLLOUT) && !c->closing) {
                conn_flush(c);
            }
        }
    }
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s [-h host] [-p port] [-v]\n", prog);
}

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    const char *port = "20000";
    bool verbose = false;
    int opt;
    while ((opt = getopt(argc, argv, "h:p:v")) != -1) {
        switch (opt) {
        case 'h':
            host = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        case 'v':
            verbose = true;
            break;
        default:
            usage(argv[0]);
            return 2;
        }
    }
    if (optind != argc) {
        usage(argv[0]);
        return 2;
    }

    /* 向已断开的连接 send 会产生 SIGPIPE，默认动作是终止整个进程；
     * 忽略它之后，send 改为返回 EPIPE，只影响那一个连接。 */
    signal(SIGPIPE, SIG_IGN);
    /* Ctrl-C 或 kill 时先跳出主循环、关闭所有连接再退出。
     * 不设 SA_RESTART，好让阻塞中的 poll 以 EINTR 返回。 */
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* 连接表里有 32 个各 16 KiB 的发送缓冲区，放在静态存储区而不是栈上。 */
    static chat_server srv;
    srv.verbose = verbose;
    for (int id = 0; id < ROOM_CAPACITY; id++) {
        srv.conns[id].fd = -1;
    }
    room_init(&srv.room, deliver, &srv);

    srv.listen_fd = net_listen(host, port);
    if (srv.listen_fd < 0) {
        return 1;
    }
    /* 监听套接字也设为非阻塞：poll 报告可读之后、accept 之前，客户端可能已经
     * 断开并被内核撤出队列，阻塞的 accept 就会一直等下一个连接。 */
    if (net_set_nonblocking(srv.listen_fd) < 0) {
        perror("fcntl");
        return 1;
    }
    fprintf(stderr, "listening on %s:%s\n", host, port);

    int rc = run(&srv);

    for (int id = 0; id < ROOM_CAPACITY; id++) {
        if (srv.conns[id].fd >= 0) {
            close(srv.conns[id].fd);
        }
    }
    close(srv.listen_fd);
    fprintf(stderr, "server stopped\n");
    return rc == 0 ? 0 : 1;
}
