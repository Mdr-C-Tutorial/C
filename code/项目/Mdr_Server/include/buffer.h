#ifndef MDR_SERVER_BUFFER_H
#define MDR_SERVER_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

// 定长字节缓冲区。
//
// 有效数据位于 data[start, end)：新数据追加在 end 处，处理完的数据从
// start 处丢弃。容量在初始化时确定，之后不再增长。这样每个连接最多占用
// 多少内存，启动时就能算出来，某个客户端发来再多数据也不会让它膨胀。
//
//   0              start            end            capacity
//   |-- 已处理的 --|---- 待处理 ----|---- 空闲 ----|
struct buffer {
    char *data;
    size_t capacity;
    size_t start;
    size_t end;
};

bool buffer_init(struct buffer *buf, size_t capacity);
void buffer_free(struct buffer *buf);
void buffer_clear(struct buffer *buf);

// 待处理的数据。
const char *buffer_data(const struct buffer *buf);
size_t buffer_length(const struct buffer *buf);
bool buffer_is_full(const struct buffer *buf);

// 尾部的空闲空间：先把数据写到 buffer_tail() 处，再用 buffer_commit() 登记。
char *buffer_tail(struct buffer *buf);
size_t buffer_space(const struct buffer *buf);
void buffer_commit(struct buffer *buf, size_t n);

// 丢弃开头 n 个已处理的字节。
void buffer_consume(struct buffer *buf, size_t n);

// 把待处理的数据挪到开头，使尾部的空闲空间最大。
void buffer_compact(struct buffer *buf);

// 把 n 个字节复制到尾部；空间不够时什么也不做，返回 false。
bool buffer_append(struct buffer *buf, const void *src, size_t n);

#endif
