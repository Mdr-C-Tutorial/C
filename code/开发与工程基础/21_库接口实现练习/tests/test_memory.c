#include "check.h"
#include "mdr_memory.h"

static void test_memset(void) {
    unsigned char buf[8];

    CHECK(mdr_memset(buf, 0xA5, sizeof(buf)) == buf);
    for (size_t i = 0; i < sizeof(buf); i++) {
        CHECK_EQ_SIZE(buf[i], 0xA5u);
    }

    CHECK(mdr_memset(buf + 2, 0, 4u) == buf + 2);
    CHECK_EQ_SIZE(buf[1], 0xA5u);
    CHECK_EQ_SIZE(buf[2], 0u);
    CHECK_EQ_SIZE(buf[5], 0u);
    CHECK_EQ_SIZE(buf[6], 0xA5u);

    CHECK(mdr_memset(buf, 0x1FF, 1u) == buf);
    CHECK_EQ_SIZE(buf[0], 0xFFu);
}

static void test_memcpy(void) {
    unsigned char buf[8];
    const unsigned char src[6] = {10u, 20u, 30u, 40u, 50u, 60u};

    CHECK(mdr_memset(buf, 0xA5, sizeof(buf)) == buf);
    CHECK(mdr_memcpy(buf + 1, src, sizeof(src)) == buf + 1);
    CHECK_EQ_SIZE(buf[0], 0xA5u);
    for (size_t i = 0; i < sizeof(src); i++) {
        CHECK_EQ_SIZE(buf[1 + i], src[i]);
    }
    CHECK_EQ_SIZE(buf[7], 0xA5u);

    CHECK(mdr_memcpy(buf, src, 0u) == buf);
    CHECK_EQ_SIZE(buf[0], 0xA5u);
}


static void test_memmove(void) {
    unsigned char buf[8];

    /* 目标在源之前：正向复制 */
    memcpy(buf, "abcdefg", 8u);
    CHECK(mdr_memmove(buf, buf + 1, 6u) == buf);
    CHECK(memcmp(buf, "bcdefg", 6u) == 0);
    CHECK_EQ_SIZE(buf[6], 'g');

    /* 目标在源之后：必须反向复制 */
    memcpy(buf, "abcdefg", 8u);
    CHECK(mdr_memmove(buf + 1, buf, 6u) == buf + 1);
    CHECK(memcmp(buf, "aabcdef", 8u) == 0);

    /* 重叠区域自身：结果不变 */
    memcpy(buf, "abcdefg", 8u);
    CHECK(mdr_memmove(buf + 2, buf + 2, 4u) == buf + 2);
    CHECK(memcmp(buf, "abcdefg", 8u) == 0);

    CHECK(mdr_memmove(buf, buf, 0u) == buf);
}

static void test_memcmp(void) {
    CHECK(mdr_memcmp("abc", "abc", 3u) == 0);
    CHECK(mdr_memcmp("abc", "abd", 3u) < 0);
    CHECK(mdr_memcmp("abd", "abc", 3u) > 0);
    CHECK(mdr_memcmp("abc", "abd", 2u) == 0);
    CHECK(mdr_memcmp("\x80", "\x7F", 1u) > 0);
    CHECK(mdr_memcmp("abc", "abd", 0u) == 0);

    {
        const unsigned char zeros[3] = {0, 0, 0};

        CHECK(mdr_memcmp(zeros, "abc", 3u) < 0);
    }
}

static void test_memchr(void) {
    const unsigned char buf[] = {'a', 0, 'b', 'c'};

    CHECK(mdr_memchr(buf, 'a', sizeof(buf)) == buf);
    CHECK(mdr_memchr(buf, 0, sizeof(buf)) == buf + 1);
    CHECK(mdr_memchr(buf, 'c', sizeof(buf)) == buf + 3);
    CHECK(mdr_memchr(buf, 'z', sizeof(buf)) == NULL);
    CHECK(mdr_memchr(buf, 'c', 3u) == NULL);
    CHECK(mdr_memchr(buf, 'a', 0u) == NULL);
}

static void test_memccpy(void) {
    unsigned char buf[8];
    const char *src = "abc:def";

    memset(buf, '#', sizeof buf);
    CHECK(mdr_memccpy(buf, src, ':', sizeof buf) == buf + 4);
    CHECK(memcmp(buf, "abc:", 4u) == 0);
    CHECK_EQ_SIZE(buf[4], '#');

    memset(buf, '#', sizeof buf);
    CHECK(mdr_memccpy(buf, src, '\0', sizeof buf) == buf + 8);
    CHECK(memcmp(buf, "abc:def", 7u) == 0);

    memset(buf, '#', sizeof buf);
    CHECK(mdr_memccpy(buf, src, 'z', 3u) == NULL);
    CHECK(memcmp(buf, "abc", 3u) == 0);
    CHECK_EQ_SIZE(buf[3], '#');
}

static void test_memset_explicit(void) {
    unsigned char buf[4];

    CHECK(mdr_memset_explicit(buf, 0x5A, sizeof(buf)) == buf);
    for (size_t i = 0; i < sizeof(buf); i++) {
        CHECK_EQ_SIZE(buf[i], 0x5Au);
    }
    CHECK(mdr_memset_explicit(buf, 0, 0u) == buf);
}

int main(void) {
    test_memset();
    test_memcpy();
    test_memmove();
    test_memcmp();
    test_memchr();
    test_memccpy();
    test_memset_explicit();
    return check_report("test_memory");
}