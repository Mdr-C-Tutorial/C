#ifndef NET_H
#define NET_H

/*
 * 套接字的几个固定步骤：解析地址、创建、绑定或连接。这里只收拢 getaddrinfo
 * 的候选地址循环和错误打印，返回的仍是普通的文件描述符，调用方照常对它使用
 * recv、send、poll 和 close。
 */

#include <stddef.h>

/* 在 host:port 上监听 TCP 连接。失败时打印原因并返回 -1。 */
int net_listen(const char *host, const char *port);

/* 连接到 host:port。失败时打印原因并返回 -1。 */
int net_connect(const char *host, const char *port);

/* 设为非阻塞模式：没有数据可读、或发送缓冲区已满时，recv、send、accept
 * 立即以 EAGAIN（或 EWOULDBLOCK）失败，而不是让进程睡下去。 */
int net_set_nonblocking(int fd);

/* 在阻塞套接字上把 len 个字节全部发出。成功返回 0，失败返回 -1。 */
int net_send_all(int fd, const void *data, size_t len);

/* 把对端地址写成 "127.0.0.1:54321" 的形式，用于日志。 */
void net_peer_name(int fd, char *out, size_t cap);

#endif
