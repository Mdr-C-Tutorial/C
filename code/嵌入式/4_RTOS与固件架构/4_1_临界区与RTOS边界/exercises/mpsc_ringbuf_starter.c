/*
 * exercises/mpsc_ringbuf_starter.c：习题 60415 起始代码。
 *
 * 任务：
 *   1. 补全 mpsc_rb_put() 与 mpsc_rb_get()，用 atomic_compare_exchange_weak_explicit
 *      预留写入槽位，并用 ready 数组提交槽位就绪状态。
 *   2. 在主机上编译运行本文件（MinGW gcc -std=c17 -O2 -Wall -Wextra -pthread），
 *      用两个写线程并发写入、主线程消费并检验所有记录不丢不重。
 */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define RB_CAP 256u
#define ITEMS_PER_WRITER 10000u

struct mpsc_ringbuf {
    uint32_t *buf;
    atomic_uchar *ready;
    size_t cap;
    size_t mask;
    atomic_size_t write_idx;
    atomic_size_t read_idx;
};

static bool mpsc_rb_init(struct mpsc_ringbuf *rb,
                         uint32_t *buf,
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

static bool mpsc_rb_put(struct mpsc_ringbuf *rb, uint32_t val) {
    /* TODO: 用 CAS 预留 write_idx 槽位，写入 buf[slot] 后以 release 置 ready[slot] = 1 */
    (void)rb;
    (void)val;
    return false;
}

static bool mpsc_rb_get(struct mpsc_ringbuf *rb, uint32_t *out) {
    /* TODO: 检查 ready[slot] 是否为 1（acquire），读出后清零 ready[slot] 并推进 read_idx */
    (void)rb;
    (void)out;
    return false;
}

struct writer_arg {
    struct mpsc_ringbuf *rb;
    uint32_t writer_id;
};

static void *writer_thread(void *arg) {
    struct writer_arg *wa = (struct writer_arg *)arg;
    for (uint32_t i = 0u; i < ITEMS_PER_WRITER; ++i) {
        uint32_t token = (wa->writer_id << 24) | i;
        while (!mpsc_rb_put(wa->rb, token)) {
            /* 缓冲区满时自旋重试 */
        }
    }
    return NULL;
}

int main(void) {
    uint32_t backing[RB_CAP];
    atomic_uchar ready[RB_CAP];
    struct mpsc_ringbuf rb;
    bool ok = mpsc_rb_init(&rb, backing, ready, RB_CAP);
    assert(ok);

    pthread_t th1;
    pthread_t th2;
    struct writer_arg a1 = { &rb, 1u };
    struct writer_arg a2 = { &rb, 2u };

    pthread_create(&th1, NULL, writer_thread, &a1);
    pthread_create(&th2, NULL, writer_thread, &a2);

    uint32_t expected_seq[3] = { 0u, 0u, 0u };
    uint32_t total_read = 0u;

    while (total_read < 2u * ITEMS_PER_WRITER) {
        uint32_t val = 0u;
        if (mpsc_rb_get(&rb, &val)) {
            uint32_t wid = val >> 24;
            uint32_t seq = val & 0x00FFFFFFu;
            assert(wid == 1u || wid == 2u);
            assert(seq == expected_seq[wid]);
            expected_seq[wid]++;
            total_read++;
        }
    }

    pthread_join(th1, NULL);
    pthread_join(th2, NULL);
    printf("verified %u records from 2 writers without loss or reorder\n",
           total_read);
    return 0;
}
