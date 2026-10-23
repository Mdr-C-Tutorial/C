#include "diff_util.h"
#include "mdr_bits.h"

#include <stdio.h>

/* 参照：显然正确的慢版本，每一位单独看。 */
static unsigned slow_clz_u32(uint32_t x) {
    unsigned n = 0;

    if (x == 0) {
        return 32;
    }
    for (uint32_t bit = 0x80000000u; (x & bit) == 0; bit >>= 1) {
        n++;
    }
    return n;
}

static unsigned slow_ctz_u32(uint32_t x) {
    unsigned n = 0;

    if (x == 0) {
        return 32;
    }
    while ((x & 1u) == 0) {
        x >>= 1;
        n++;
    }
    return n;
}

static unsigned slow_popcount_u32(uint32_t x) {
    unsigned n = 0;

    while (x != 0) {
        n += (unsigned)(x & 1u);
        x >>= 1;
    }
    return n;
}

/* 65536 个 16 位输入：位函数的全部长度类别都在这段里。 */
static void check_u16(uint32_t x) {
    unsigned want;
    unsigned got;

    want = slow_clz_u32(x);
    got = mdr_clz_u32(x);
    if (want != got) {
        diff_report("clz_u32", &x, sizeof x, (long)want, (long)got);
    } else {
        diff_ok();
    }

    want = slow_ctz_u32(x);
    got = mdr_ctz_u32(x);
    if (want != got) {
        diff_report("ctz_u32", &x, sizeof x, (long)want, (long)got);
    } else {
        diff_ok();
    }

    want = slow_popcount_u32(x);
    got = mdr_popcount_u32(x);
    if (want != got) {
        diff_report("popcount_u32", &x, sizeof x, (long)want, (long)got);
    } else {
        diff_ok();
    }
}

int main(void) {
    for (uint32_t x = 0; x <= 0xFFFFu; x++) {
        check_u16(x);
    }
    return diff_summary("diff_bits");
}