/*
 * mpsc_reserve.h：多个不同优先级中断共同写入、主循环读取的环形缓冲区（MPSC）。
 *
 * 设计要点：
 *   1. 写入方通过 atomic_compare_exchange_weak_explicit 争用 write_idx 预留槽位。
 *   2. 由于低优先级中断在预留槽位后、写入数据前可能被高优先级中断抢占，
 *      消费方不能仅凭 write_idx 判断数据已就绪，必须等待对应槽位的 ready[slot] 置 1。
 */
#ifndef MPSC_RESERVE_H
#define MPSC_RESERVE_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mpsc_ringbuf {
    uint8_t *buf;
    atomic_uchar *ready;
    size_t cap;
    size_t mask;
    atomic_size_t write_idx;
    atomic_size_t read_idx;
};

bool mpsc_rb_init(struct mpsc_ringbuf *rb,
                  uint8_t *buf,
                  atomic_uchar *ready,
                  size_t cap);
bool mpsc_rb_put(struct mpsc_ringbuf *rb, uint8_t byte);
bool mpsc_rb_get(struct mpsc_ringbuf *rb, uint8_t *out);

#endif
