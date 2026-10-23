/*
 * test_mpsc.c：在 PC 上用两个写线程模拟两个不同优先级的中断，验证
 * mpsc_reserve.c 的多生产者单消费者缓冲区不丢、不重、每个写者内部有序。
 *
 * 编译与运行（MinGW gcc 需加 -pthread）：
 *   gcc -std=c17 -O2 -Wall -Wextra -pthread test_mpsc.c mpsc_reserve.c -o test_mpsc
 *   ./test_mpsc
 */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "mpsc_reserve.h"

#define RB_CAP 256u
#define ITEMS_PER_WRITER 10000u

struct writer_arg {
    struct mpsc_ringbuf *rb;
    uint8_t base_tag;
};

static void *writer_thread(void *arg) {
    struct writer_arg *wa = (struct writer_arg *)arg;
    for (uint32_t i = 0u; i < ITEMS_PER_WRITER; ++i) {
        uint8_t b = (uint8_t)(wa->base_tag + (i & 0x3Fu));
        while (!mpsc_rb_put(wa->rb, b)) {
        }
    }
    return NULL;
}

int main(void) {
    uint8_t backing[RB_CAP];
    atomic_uchar ready[RB_CAP];
    struct mpsc_ringbuf rb;
    bool ok = mpsc_rb_init(&rb, backing, ready, RB_CAP);
    assert(ok);

    pthread_t th1;
    pthread_t th2;
    struct writer_arg a1 = { &rb, 0x40u };
    struct writer_arg a2 = { &rb, 0x80u };

    pthread_create(&th1, NULL, writer_thread, &a1);
    pthread_create(&th2, NULL, writer_thread, &a2);

    uint32_t c1 = 0u;
    uint32_t c2 = 0u;
    while (c1 + c2 < 2u * ITEMS_PER_WRITER) {
        uint8_t out = 0u;
        if (mpsc_rb_get(&rb, &out)) {
            if ((out & 0xC0u) == 0x40u) {
                assert((out & 0x3Fu) == (c1 & 0x3Fu));
                c1++;
            } else {
                assert((out & 0xC0u) == 0x80u);
                assert((out & 0x3Fu) == (c2 & 0x3Fu));
                c2++;
            }
        }
    }

    pthread_join(th1, NULL);
    pthread_join(th2, NULL);
    printf("MPSC host test passed: writer1=%u, writer2=%u\n", c1, c2);
    return 0;
}
