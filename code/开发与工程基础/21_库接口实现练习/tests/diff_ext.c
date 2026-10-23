#include "diff_util.h"
#include "mdr_ext.h"

#include <stdio.h>
#include <string.h>

/* 防护字节检查：mdr_strlcpy 与 mdr_strlcat 只在 dsize 个字节内写，
 * 数据区正好按 dsize 分配，前后各 16 个 0xA5，调用后检查。 */

static void check_strlcpy(const char *dst0, const char *src) {
    size_t dsize = strlen(dst0) + 1;
    diff_guard guard;

    if (diff_guard_init(&guard, dsize) != 0) {
        return;
    }
    memcpy(guard.data, dst0, dsize);
    size_t n = mdr_strlcpy((char *)guard.data, src, dsize);

    if (diff_guard_check(&guard, "strlcpy") == 0) {
        size_t want = strlen(src) < dsize - 1 ? strlen(src) : dsize - 1;

        if (n == strlen(src) && memcmp(guard.data, src, want) == 0) {
            diff_ok();
        } else {
            diff_report("strlcpy result", guard.data, dsize, (long)strlen(src),
                        (long)n);
        }
    }
    diff_guard_free(&guard);
}

static void check_strlcat(const char *dst0, const char *src) {
    size_t dsize = strlen(dst0) + strlen(src) + 1;
    diff_guard guard;

    if (diff_guard_init(&guard, dsize) != 0) {
        return;
    }
    memcpy(guard.data, dst0, strlen(dst0) + 1);
    size_t n = mdr_strlcat((char *)guard.data, src, dsize);

    if (diff_guard_check(&guard, "strlcat") == 0) {
        size_t want = strlen(dst0) + strlen(src);

        if (n == want && strcmp((char *)guard.data, "hello world") == 0) {
            diff_ok();
        } else {
            diff_report("strlcat result", guard.data, dsize, (long)want,
                        (long)n);
        }
    }
    diff_guard_free(&guard);
}

int main(void) {
    check_strlcpy("", "hello");
    check_strlcpy("h", "hello");
    check_strlcpy("hello", "hello");
    check_strlcpy("world", "hello");
    check_strlcpy("a longer string", "hello");

    check_strlcat("hello", " world");
    check_strlcat("", "hello world");

    return diff_summary("diff_ext");
}