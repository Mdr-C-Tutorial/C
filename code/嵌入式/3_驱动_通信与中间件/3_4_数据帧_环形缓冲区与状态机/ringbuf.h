#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* C11 编译期断言：确认目标平台上的整型原子操作是真正无锁的硬件指令 */
_Static_assert(ATOMIC_INT_LOCK_FREE == 2,
               "atomic int/size_t must be always lock-free");

struct ringbuf {
    uint8_t *buf;
    size_t cap;
    size_t mask;
    atomic_size_t head;
    atomic_size_t tail;
};

bool rb_init(struct ringbuf *rb, uint8_t *buf, size_t cap);
bool rb_put(struct ringbuf *rb, uint8_t byte);
bool rb_get(struct ringbuf *rb, uint8_t *out);
size_t rb_count(const struct ringbuf *rb);
size_t rb_free(const struct ringbuf *rb);

#endif
