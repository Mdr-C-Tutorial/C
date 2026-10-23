#include "ringbuf.h"
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint8_t storage[8];
    struct ringbuf rb;

    if (!rb_init(&rb, storage, sizeof(storage))) {
        printf("rb_init failed\n");
        return 1;
    }

    printf("[1] fill to capacity (cap=%u)\n", (unsigned)rb.cap);
    for (uint8_t i = 0u; i < 8u; ++i) {
        bool ok = rb_put(&rb, (uint8_t)(0x10u + i));
        if (!ok) {
            printf("unexpected full at i=%u\n", (unsigned)i);
            return 1;
        }
    }
    printf("    count=%u, free=%u, 9th put=%s\n",
           (unsigned)rb_count(&rb), (unsigned)rb_free(&rb),
           rb_put(&rb, 0x99u) ? "true" : "false");

    printf("[2] pop 5 bytes, then push 5 bytes (slot wrap-around)\n");
    for (int i = 0; i < 5; ++i) {
        uint8_t v = 0u;
        (void)rb_get(&rb, &v);
    }
    for (uint8_t i = 0u; i < 5u; ++i) {
        (void)rb_put(&rb, (uint8_t)(0xA0u + i));
    }
    size_t h2 = atomic_load(&rb.head);
    size_t t2 = atomic_load(&rb.tail);
    printf("    head=%u (slot %u), tail=%u (slot %u), count=%u\n    data:",
           (unsigned)h2, (unsigned)(h2 & rb.mask),
           (unsigned)t2, (unsigned)(t2 & rb.mask),
           (unsigned)rb_count(&rb));
    uint8_t out = 0u;
    while (rb_get(&rb, &out)) {
        printf(" %02X", (unsigned)out);
    }
    printf("\n");

    printf("[3] unsigned index wrap-around across SIZE_MAX\n");
    atomic_store(&rb.head, SIZE_MAX - 2u);
    atomic_store(&rb.tail, SIZE_MAX - 2u);
    for (uint8_t i = 1u; i <= 6u; ++i) {
        (void)rb_put(&rb, i);
    }
    size_t h3 = atomic_load(&rb.head);
    size_t t3 = atomic_load(&rb.tail);
    printf("    after 6 puts: head=%u (wrapped), tail=SIZE_MAX-%u, head-tail=%u\n    data:",
           (unsigned)h3, (unsigned)(SIZE_MAX - t3), (unsigned)rb_count(&rb));
    while (rb_get(&rb, &out)) {
        printf(" %02X", (unsigned)out);
    }
    printf("\n");

    return 0;
}
