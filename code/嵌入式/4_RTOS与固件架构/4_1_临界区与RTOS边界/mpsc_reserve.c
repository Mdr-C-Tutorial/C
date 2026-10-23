/*
 * mpsc_reserve.c：用 CAS 预留写入位置并提交槽位就绪标志的关键实现。
 */
#include "mpsc_reserve.h"

bool mpsc_rb_init(struct mpsc_ringbuf *rb,
                  uint8_t *buf,
                  atomic_uchar *ready,
                  size_t cap) {
    if (rb == NULL || buf == NULL || ready == NULL ||
        cap == 0u || (cap & (cap - 1u)) != 0u) {
        return false;
    }
    rb->buf = buf;
    rb->ready = ready;
    rb->cap = cap;
    rb->mask = cap - 1u;
    atomic_init(&rb->write_idx, 0u);
    atomic_init(&rb->read_idx, 0u);
    for (size_t i = 0u; i < cap; ++i) {
        atomic_init(&rb->ready[i], 0u);
    }
    return true;
}

bool mpsc_rb_put(struct mpsc_ringbuf *rb, uint8_t byte) {
    size_t w = atomic_load_explicit(&rb->write_idx, memory_order_relaxed);

    for (;;) {
        size_t r = atomic_load_explicit(&rb->read_idx, memory_order_acquire);
        if (w - r >= rb->cap) {
            return false; /* 缓冲区已满 */
        }

        /* 尝试预留槽位 w；若被高优先级中断抢先，w 会被更新为最新值并重试 */
        if (atomic_compare_exchange_weak_explicit(
                &rb->write_idx, &w, w + 1u,
                memory_order_relaxed, memory_order_relaxed)) {
            break;
        }
    }

    size_t slot = w & rb->mask;
    rb->buf[slot] = byte;
    /* 数据写入完毕后再将槽位标为就绪，供主循环按序读取 */
    atomic_store_explicit(&rb->ready[slot], 1u, memory_order_release);
    return true;
}

bool mpsc_rb_get(struct mpsc_ringbuf *rb, uint8_t *out) {
    size_t r = atomic_load_explicit(&rb->read_idx, memory_order_relaxed);
    size_t slot = r & rb->mask;

    /* 若槽位尚未被预留它的写者提交，说明缓冲区为空或低优先级写者正被抢占 */
    if (atomic_load_explicit(&rb->ready[slot], memory_order_acquire) == 0u) {
        return false;
    }

    *out = rb->buf[slot];
    atomic_store_explicit(&rb->ready[slot], 0u, memory_order_relaxed);
    atomic_store_explicit(&rb->read_idx, r + 1u, memory_order_release);
    return true;
}
