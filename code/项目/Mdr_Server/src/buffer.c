#include "buffer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

bool buffer_init(struct buffer *buf, size_t capacity) {
    buf->data = malloc(capacity);
    buf->capacity = buf->data != NULL ? capacity : 0;
    buf->start = 0;
    buf->end = 0;
    return buf->data != NULL;
}

void buffer_free(struct buffer *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->capacity = 0;
    buf->start = 0;
    buf->end = 0;
}

void buffer_clear(struct buffer *buf) {
    buf->start = 0;
    buf->end = 0;
}

const char *buffer_data(const struct buffer *buf) {
    return buf->data + buf->start;
}

size_t buffer_length(const struct buffer *buf) { return buf->end - buf->start; }

bool buffer_is_full(const struct buffer *buf) {
    return buffer_length(buf) == buf->capacity;
}

char *buffer_tail(struct buffer *buf) { return buf->data + buf->end; }

size_t buffer_space(const struct buffer *buf) {
    return buf->capacity - buf->end;
}

void buffer_commit(struct buffer *buf, size_t n) {
    assert(n <= buffer_space(buf));
    buf->end += n;
}

void buffer_consume(struct buffer *buf, size_t n) {
    assert(n <= buffer_length(buf));
    buf->start += n;
    // 数据全部处理完是最常见的情况，这时直接回到开头，省掉以后的 memmove。
    if (buf->start == buf->end) {
        buf->start = 0;
        buf->end = 0;
    }
}

void buffer_compact(struct buffer *buf) {
    if (buf->start == 0) {
        return;
    }
    size_t length = buffer_length(buf);
    // 源区域和目标区域可能重叠，只能用 memmove。
    memmove(buf->data, buf->data + buf->start, length);
    buf->start = 0;
    buf->end = length;
}

bool buffer_append(struct buffer *buf, const void *src, size_t n) {
    if (n > buffer_space(buf)) {
        return false;
    }
    memcpy(buffer_tail(buf), src, n);
    buf->end += n;
    return true;
}
