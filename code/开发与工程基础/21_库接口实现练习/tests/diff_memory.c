#include "diff_util.h"
#include "mdr_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 随机差分测试：长度、内容与改动位置都由 splitmix64 决定。
 * 用法：diff_memory [--seed N] [--rounds N] */

static void fill_random(unsigned char *buf, size_t len, uint64_t *state) {
    for (size_t i = 0; i < len; i++) {
        uint64_t r = diff_random(state);

        /* 每四个字节里安排一个特殊值，保证 0x00 和 0x80 以上的字节都出现。 */
        switch (i % 4) {
        case 0:
            buf[i] = (unsigned char)(r & 0xFFu);
            break;
        case 1:
            buf[i] = 0x00;
            break;
        case 2:
            buf[i] = 0x80;
            break;
        default:
            buf[i] = (unsigned char)(r & 0x7Fu);
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    uint64_t seed = 1;
    int rounds = 1000;
    uint64_t state;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = diff_parse_seed(argv[++i]);
        } else if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) {
            rounds = atoi(argv[++i]);
        }
    }
    printf("seed = 0x%llX, rounds = %d\n", (unsigned long long)seed, rounds);

    state = seed;
    for (int round = 0; round < rounds; round++) {
        size_t len = (size_t)(diff_random(&state) % 65u);
        unsigned char lhs[64];
        unsigned char rhs[64];
        int want;
        int got;

        fill_random(lhs, len, &state);
        memcpy(rhs, lhs, len);
        if (len > 0 && diff_random(&state) % 3u == 0) {
            size_t pos = (size_t)(diff_random(&state) % len);

            rhs[pos] = (unsigned char)(rhs[pos] ^ 0x80u);
        }
        want = diff_sign(memcmp(lhs, rhs, len));
        got = diff_sign(mdr_memcmp(lhs, rhs, len));
        if (want != got) {
            diff_report("memcmp", lhs, len, want, got);
        } else {
            diff_ok();
        }
    }
    return diff_summary("diff_memory");
}