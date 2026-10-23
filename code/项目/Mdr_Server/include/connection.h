#ifndef MDR_SERVER_CONNECTION_H
#define MDR_SERVER_CONNECTION_H

#include "buffer.h"
#include "timer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 每个连接的输出缓冲区大小，也是每次从文件读取的最大字节数。
#define CONN_OUTPUT_BUFFER_SIZE 16384

struct server;

// 连接的状态。每个状态在 epoll 中只登记一种事件，见 connection.c 开头。
enum conn_state {
    CONN_FREE,      // 连接池里的空槽位
    CONN_READING,   // 读请求：等请求头收齐，或等长连接上的下一个请求
    CONN_WRITING,   // 写响应：先是响应头，然后是文件内容
    CONN_LINGERING, // 已关闭写方向，读掉对方剩下的数据，等对方关闭
    CONN_CLOSED,    // 已决定关闭，等事件循环回收槽位
};

struct connection {
    enum conn_state state;
    int fd;
    uint32_t events;         // 当前在 epoll 中登记的事件
    struct buffer in;        // 收到了、还没处理的请求数据
    struct buffer out;       // 等待写出的响应数据
    int file_fd;             // 正在发送的文件，没有时为 -1
    uint64_t file_remaining; // 文件中还没读进 out 的字节数
    bool keep_alive;         // 当前响应发完后是否继续读下一个请求
    struct timer idle_timer; // 一段时间没有任何进展就关闭连接
    char peer[32];           // 对端的 "IP:端口"，用于日志
    struct server *server;
    struct connection *next_free; // 槽位空闲时串在空闲链表上
};

// 连接池槽位的初始化与释放。缓冲区在启动时一次分配好，之后反复使用。
bool connection_slot_init(struct connection *conn, size_t input_size);
void connection_slot_free(struct connection *conn);

// accept 得到新套接字之后调用：初始化槽位，登记到 epoll，启动空闲定时器。
bool connection_open(struct connection *conn, struct server *srv, int fd,
                     const char *peer);

// 连接上发生了事件（可读、可写或出错）。返回后如果 state 变成了
// CONN_CLOSED，由调用者释放连接。
void connection_handle_event(struct connection *conn);

// 是否处于两个请求之间：没有任何收到了还没处理的数据。
bool connection_is_idle(const struct connection *conn);

// 关闭套接字和文件，取消定时器，槽位回到 CONN_FREE。
void connection_close(struct connection *conn);

#endif
