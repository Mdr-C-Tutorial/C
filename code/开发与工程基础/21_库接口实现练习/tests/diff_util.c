#include "diff_util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t diff_total = 0;
static size_t diff_failed = 0;
static size_t diff_printed = 0;

int diff_sign(int value) {
    if (value < 0) {
        return -1;
    }
    if (value > 0) {
        return 1;
    }
    return 0;
}

long diff_offset(const void *base, const void *found) {
    if (found == NULL) {
        return -1;
    }
    return (long)((const char *)found - (const char *)base);
}

void diff_escape(char *out, size_t cap, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    size_t used = 0;

    if (cap < 3) {
        if (cap > 0) {
            out[0] = '\0';
        }
        return;
    }
    out[used++] = '"';
    for (size_t i = 0; i < len && used + 5 < cap; i++) {
        unsigned char c = p[i];

        if (c == '"' || c == '\\') {
            out[used++] = '\\';
            out[used++] = (char)c;
        } else if (isprint(c)) {
            out[used++] = (char)c;
        } else {
            used += (size_t)snprintf(out + used, cap - used, "\\x%02X", c);
        }
    }
    out[used++] = '"';
    out[used] = '\0';
}

void diff_dump(const char *label, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;

    printf("  %-8s", label);
    for (size_t i = 0; i < len && i < 32; i++) {
        printf(" %02X", p[i]);
    }
    if (len > 32) {
        printf(" ... (%zu bytes)", len);
    }
    printf("\n");
}

void diff_ok(void) {
    diff_total++;
}

int diff_report(const char *what, const void *input, size_t input_len,
                long expected, long actual) {
    diff_total++;
    diff_failed++;
    if (diff_printed < 5) {
        char text[128];

        diff_printed++;
        diff_escape(text, sizeof text, input, input_len);
        printf("MISMATCH %s: input %s\n", what, text);
        diff_dump("hex", input, input_len);
        printf("  expected %ld, got %ld\n", expected, actual);
    }
    return 1;
}

int diff_summary(const char *suite) {
    printf("%s: %zu comparisons, %zu failed\n", suite, diff_total, diff_failed);
    return diff_failed == 0 ? 0 : 1;
}

int diff_guard_init(diff_guard *guard, size_t size) {
    guard->raw = (unsigned char *)malloc(size + 2 * DIFF_GUARD);
    if (guard->raw == NULL) {
        return -1;
    }
    for (size_t i = 0; i < DIFF_GUARD; i++) {
        guard->raw[i] = 0xA5;
        guard->raw[DIFF_GUARD + size + i] = 0xA5;
    }
    guard->data = guard->raw + DIFF_GUARD;
    guard->size = size;
    return 0;
}

void diff_guard_free(diff_guard *guard) {
    free(guard->raw);
    guard->raw = NULL;
    guard->data = NULL;
    guard->size = 0;
}

int diff_guard_check(const diff_guard *guard, const char *what) {
    const unsigned char *front = guard->raw;
    const unsigned char *back = guard->raw + DIFF_GUARD + guard->size;

    diff_total++;
    for (size_t i = 0; i < DIFF_GUARD; i++) {
        if (front[i] != 0xA5) {
            diff_failed++;
            printf("GUARD %s: front[%zu] is %02X, expected A5\n", what, i,
                   front[i]);
            return 1;
        }
    }
    for (size_t i = 0; i < DIFF_GUARD; i++) {
        if (back[i] != 0xA5) {
            diff_failed++;
            printf("GUARD %s: data[%zu] is %02X, expected A5\n", what,
                   guard->size + i, back[i]);
            return 1;
        }
    }
    return 0;
}

uint64_t diff_random(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);

    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint64_t diff_parse_seed(const char *text) {
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, 0);

    if (end == text || *end != '\0') {
        fprintf(stderr, "invalid seed: %s\n", text);
        exit(2);
    }
    return (uint64_t)value;
}

size_t diff_shrink(unsigned char *data, size_t len, diff_fail_fn fails,
                   void *ctx) {
    size_t i = 0;

    while (i < len) {
        unsigned char saved = data[i];

        memmove(data + i, data + i + 1, len - i - 1);
        if (fails(data, len - 1, ctx) != 0) {
            len--;
            continue;
        }
        memmove(data + i + 1, data + i, len - i - 1);
        data[i] = 0x00;
        if (fails(data, len, ctx) != 0) {
            i++;
            continue;
        }
        data[i] = (unsigned char)'a';
        if (fails(data, len, ctx) != 0) {
            i++;
            continue;
        }
        data[i] = saved;
        i++;
    }
    return len;
}