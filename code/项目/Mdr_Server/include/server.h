#ifndef MDR_SERVER_SERVER_H
#define MDR_SERVER_SERVER_H

#include "connection.h"
#include "timer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum log_level {
    LOG_ERROR, // 错误，总是输出
    LOG_INFO,  // 启动、退出和每个请求的访问日志（默认）
    LOG_DEBUG, // 另外输出连接的建立、关闭和超时（-v）
};

struct server_config {
    const char *address; // 监听的 IPv4 地址
    int port;
    const char *root; // 文档根目录
    int max_connections;
    size_t max_request_size; // 请求头的最大字节数，即输入缓冲区的大小
    int idle_timeout_ms;
    enum log_level log_level;
};

struct server {
    struct server_config config;
    int listen_fd;
    int epoll_fd;
    int signal_fd;
    int root_fd;                    // 文档根目录，文件都相对它用 openat 打开
    struct connection *connections; // 连接池，共 max_connections 个槽位
    struct connection *free_list;   // 空闲槽位组成的链表
    int active_connections;
    bool accepting; // 监听套接字是否登记在 epoll 中
    struct timer_queue idle_timers;
    int64_t now_ms;          // 本轮事件处理开始时的单调时钟读数
    bool shutdown_requested; // 收到了第一个 SIGINT 或 SIGTERM
    bool shutting_down;      // 正在优雅退出：不再接受新连接
    bool stop;               // 立即离开事件循环
};

// 失败时返回 -1。无论成功与否，最后都要调用 server_cleanup。
int server_init(struct server *srv, const struct server_config *config);
int server_run(struct server *srv);
void server_cleanup(struct server *srv);

void server_log(const struct server *srv, enum log_level level,
                const char *format, ...);

#endif
