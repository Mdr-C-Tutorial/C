#include "../ringbuf.h"
#include <stdio.h>
#include <string.h>

/*
 * TODO：实现批量写入 rb_write 与批量读取 rb_read。
 * 要求：
 * 1. 计算当前可用空间（或可读字节数）与请求长度 len 的较小值 n。
 * 2. 若写（或读）跨越了底层数组末尾（pos + n > rb->cap），拆成两段 memcpy：
 *    第一段长度为 first = rb->cap - pos，第二段从 buf[0] 开始复制 n - first 字节。
 * 3. 全部复制完成后，只执行一次 atomic_store_explicit(..., memory_order_release)
 *    把 head（或 tail）推进 n。
 * 4. 返回实际写入（或读出）的字节数 n。
 */
size_t rb_write(struct ringbuf *rb, const uint8_t *data, size_t len) {
    (void)rb;
    (void)data;
    (void)len;
    return 0u;
}

size_t rb_read(struct ringbuf *rb, uint8_t *out, size_t len) {
    (void)rb;
    (void)out;
    (void)len;
    return 0u;
}

int main(void) {
    uint8_t mem[8];
    struct ringbuf rb;
    rb_init(&rb, mem, sizeof(mem));

    const uint8_t a[6] = { 1, 2, 3, 4, 5, 6 };
    uint8_t out[8] = {0};

    size_t w1 = rb_write(&rb, a, 6u);
    size_t r1 = rb_read(&rb, out, 4u);
    /* 此时 tail=4, head=6，再写入 6 字节将跨越末尾索引 7 回绕到 0..3 */
    const uint8_t b[6] = { 10, 20, 30, 40, 50, 60 };
    size_t w2 = rb_write(&rb, b, 6u);
    size_t r2 = rb_read(&rb, out, 8u);

    printf("w1=%u r1=%u w2=%u r2=%u\ndata:",
           (unsigned)w1, (unsigned)r1, (unsigned)w2, (unsigned)r2);
    for (size_t i = 0u; i < r2; ++i) {
        printf(" %u", (unsigned)out[i]);
    }
    printf("\n");
    return 0;
}
