#include "mdr_compiler.h"
#include "mdr_memory.h"

#include <stdint.h>

void *mdr_memset(void *dest, int ch, size_t count) {
    unsigned char *p = (unsigned char *)dest;
    unsigned char byte = (unsigned char)ch;

    MDR_ASSERT(dest != NULL || count == 0);
    for (size_t i = 0; i < count; i++) {
        p[i] = byte;
    }
    return dest;
}

void *mdr_memcpy(void *restrict dest, const void *restrict src, size_t count) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    MDR_ASSERT(dest != NULL || count == 0);
    MDR_ASSERT(src != NULL || count == 0);
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}

/* 21.3 的内存函数在这里继续实现。 */
void *mdr_memmove(void *dest, const void *src, size_t count) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    MDR_ASSERT(dest != NULL || count == 0);
    MDR_ASSERT(src != NULL || count == 0);
    if ((uintptr_t)d < (uintptr_t)s) {
        for (size_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else if ((uintptr_t)d > (uintptr_t)s) {
        for (size_t i = count; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

int mdr_memcmp(const void *lhs, const void *rhs, size_t count) {
    const unsigned char *p1 = (const unsigned char *)lhs;
    const unsigned char *p2 = (const unsigned char *)rhs;

    MDR_ASSERT(lhs != NULL || count == 0);
    MDR_ASSERT(rhs != NULL || count == 0);
    for (size_t i = 0; i < count; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];
        }
    }
    return 0;
}

void *mdr_memchr(const void *ptr, int ch, size_t count) {
    const unsigned char *p = (const unsigned char *)ptr;
    unsigned char c = (unsigned char)ch;

    MDR_ASSERT(ptr != NULL || count == 0);
    for (size_t i = 0; i < count; i++) {
        if (p[i] == c) {
            return (void *)(p + i);
        }
    }
    return NULL;
}

void *mdr_memccpy(void *restrict dest, const void *restrict src, int c,
                  size_t count) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    unsigned char byte = (unsigned char)c;

    MDR_ASSERT(dest != NULL || count == 0);
    MDR_ASSERT(src != NULL || count == 0);
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
        if (d[i] == byte) {
            return d + i + 1;
        }
    }
    return NULL;
}

void *mdr_memset_explicit(void *dest, int ch, size_t count) {
    volatile unsigned char *p = (volatile unsigned char *)dest;
    unsigned char byte = (unsigned char)ch;

    MDR_ASSERT(dest != NULL || count == 0);
    for (size_t i = 0; i < count; i++) {
        p[i] = byte;
    }
    return dest;
}
