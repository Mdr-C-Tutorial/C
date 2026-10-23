#include "ringbuf.h"

bool rb_init(struct ringbuf *rb, uint8_t *buf, size_t cap) {
    if (rb == NULL || buf == NULL || cap == 0u || (cap & (cap - 1u)) != 0u) {
        return false;
    }
    rb->buf = buf;
    rb->cap = cap;
    rb->mask = cap - 1u;
    atomic_init(&rb->head, 0u);
    atomic_init(&rb->tail, 0u);
    return true;
}

bool rb_put(struct ringbuf *rb, uint8_t byte) {
    size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    if (head - tail == rb->cap) {
        return false;
    }

    rb->buf[head & rb->mask] = byte;
    atomic_store_explicit(&rb->head, head + 1u, memory_order_release);
    return true;
}

bool rb_get(struct ringbuf *rb, uint8_t *out) {
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);

    if (head == tail) {
        return false;
    }

    *out = rb->buf[tail & rb->mask];
    atomic_store_explicit(&rb->tail, tail + 1u, memory_order_release);
    return true;
}

size_t rb_count(const struct ringbuf *rb) {
    size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
    return head - tail;
}

size_t rb_free(const struct ringbuf *rb) {
    return rb->cap - rb_count(rb);
}
