#include "../dma_rx_pos.h"
#include <stdio.h>

#define BUF_SIZE 64u

struct test_case {
    const char *name;
    size_t old_pos;
    size_t cndtr;
    size_t expect_new_pos;
    size_t expect_total_len;
    size_t expect_spans;
    size_t expect_off0;
    size_t expect_len0;
    size_t expect_off1;
    size_t expect_len1;
};

int main(void) {
    /* TODO: 补充表驱动测试用例，覆盖不回绕、恰好到末尾、跨越末尾回绕、无新数据等情形 */
    static const struct test_case cases[] = {
        {"normal_advance", 0u, 44u, 20u, 20u, 1u, 0u, 20u, 0u, 0u},
    };

    size_t count = sizeof(cases) / sizeof(cases[0]);
    size_t passed = 0u;

    for (size_t i = 0u; i < count; ++i) {
        struct dma_rx_slice s = dma_rx_diff(BUF_SIZE, cases[i].old_pos, cases[i].cndtr);
        int ok = (s.new_pos == cases[i].expect_new_pos) &&
                 (s.total_len == cases[i].expect_total_len) &&
                 (s.span_count == cases[i].expect_spans);
        if (ok && s.span_count >= 1u) {
            ok = ok && (s.spans[0].offset == cases[i].expect_off0) &&
                 (s.spans[0].len == cases[i].expect_len0);
        }
        if (ok && s.span_count >= 2u) {
            ok = ok && (s.spans[1].offset == cases[i].expect_off1) &&
                 (s.spans[1].len == cases[i].expect_len1);
        }
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", cases[i].name);
        if (ok) {
            ++passed;
        }
    }

    printf("Passed %u / %u\n", (unsigned)passed, (unsigned)count);
    return passed == count ? 0 : 1;
}
