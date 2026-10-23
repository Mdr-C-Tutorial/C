#include "check.h"
#include "mdr_string.h"

static void test_strlen(void) {
    CHECK_EQ_SIZE(mdr_strlen(""), 0u);
    CHECK_EQ_SIZE(mdr_strlen("hello"), 5u);
    CHECK_EQ_SIZE(mdr_strlen("a\0b"), 1u);
    CHECK_EQ_SIZE(mdr_strlen("\x80\xFF"), 2u);
}

static void test_strcmp(void) {
    const char high_byte[] = {(char)0x80, '\0'};
    const char ascii_byte[] = {0x7F, '\0'};

    CHECK(mdr_strcmp("", "") == 0);
    CHECK(mdr_strcmp("abc", "abc") == 0);
    CHECK(mdr_strcmp("abc", "abd") < 0);
    CHECK(mdr_strcmp("abd", "abc") > 0);
    CHECK(mdr_strcmp("ab", "abc") < 0);
    CHECK(mdr_strcmp(high_byte, ascii_byte) > 0);
}

static void test_strnlen(void) {
    const char non_terminated[4] = {'a', 'b', 'c', 'd'};

    CHECK_EQ_SIZE(mdr_strnlen("", 4u), 0u);
    CHECK_EQ_SIZE(mdr_strnlen("hello", 0u), 0u);
    CHECK_EQ_SIZE(mdr_strnlen("hello", 3u), 3u);
    CHECK_EQ_SIZE(mdr_strnlen("hello", 10u), 5u);
    CHECK_EQ_SIZE(mdr_strnlen(non_terminated, 4u), 4u);
}

static void test_strcpy_strcat(void) {
    char buf[32];

    CHECK(mdr_strcpy(buf, "hello") == buf);
    CHECK_STR_EQ(buf, "hello");
    CHECK(mdr_strcpy(buf, "") == buf);
    CHECK_STR_EQ(buf, "");

    CHECK(mdr_strcpy(buf, "ab") == buf);
    CHECK(mdr_strcat(buf, "cd") == buf);
    CHECK_STR_EQ(buf, "abcd");
    CHECK(mdr_strcat(buf, "") == buf);
    CHECK_STR_EQ(buf, "abcd");

    CHECK(mdr_strcpy(buf, "") == buf);
    CHECK(mdr_strncat(buf, "xyz", 2u) == buf);
    CHECK_STR_EQ(buf, "xy");
    CHECK(mdr_strncat(buf, "xyz", 10u) == buf);
    CHECK_STR_EQ(buf, "xyxyz");
}

static void test_strncpy(void) {
    char buf[8];

    memset(buf, '#', sizeof buf);
    CHECK(mdr_strncpy(buf, "ab", 4u) == buf);
    CHECK_EQ_SIZE((unsigned char)buf[0], 'a');
    CHECK_EQ_SIZE((unsigned char)buf[1], 'b');
    CHECK_EQ_SIZE((unsigned char)buf[2], 0u);
    CHECK_EQ_SIZE((unsigned char)buf[3], 0u);
    CHECK_EQ_SIZE((unsigned char)buf[4], '#');

    memset(buf, '#', sizeof buf);
    CHECK(mdr_strncpy(buf, "abcdefgh", 4u) == buf);
    CHECK(memcmp(buf, "abcd", 4u) == 0);
    CHECK_EQ_SIZE((unsigned char)buf[4], '#');
}

static void test_strncmp(void) {
    const char high_byte[] = {(char)0x80, '\0'};

    CHECK(mdr_strncmp("abc", "abd", 2u) == 0);
    CHECK(mdr_strncmp("abc", "abd", 3u) < 0);
    CHECK(mdr_strncmp("abc", "abc", 10u) == 0);
    CHECK(mdr_strncmp("ab", "abc", 3u) < 0);
    CHECK(mdr_strncmp("abc", "ab", 3u) > 0);
    CHECK(mdr_strncmp(high_byte, "a", 1u) > 0);
}

/* 表驱动：每个用例给出待查字符和两个期望偏移，-1 表示找不到 */
struct chr_case {
    int ch;
    int first;
    int last;
};

static long offset_or_neg(const char *base, const char *p) {
    return p != NULL ? (long)(p - base) : -1L;
}

static void test_strchr(void) {
    static const struct chr_case cases[] = {
        {'b', 0, 0},
        {'a', 1, 5},
        {'n', 2, 4},
        {'z', -1, -1},
    };
    const char text[] = "banana";

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        CHECK(offset_or_neg(text, mdr_strchr(text, cases[i].ch)) == cases[i].first);
        CHECK(offset_or_neg(text, mdr_strrchr(text, cases[i].ch)) == cases[i].last);
    }

    /* 终止空字符也在查找范围内：strchr 返回指向它的指针 */
    CHECK(mdr_strchr(text, 0) == text + 6);
    CHECK(mdr_strrchr(text, 0) == text + 6);
}

static void test_strspn(void) {
    const char text[] = "12345abc";

    CHECK_EQ_SIZE(mdr_strspn(text, "0123456789"), 5u);
    CHECK_EQ_SIZE(mdr_strspn(text, ""), 0u);
    CHECK_EQ_SIZE(mdr_strspn("", "abc"), 0u);
    CHECK_EQ_SIZE(mdr_strspn("aabb", "ab"), 4u);
    CHECK_EQ_SIZE(mdr_strcspn(text, "abc"), 5u);
    CHECK_EQ_SIZE(mdr_strcspn(text, "xyz"), 8u);
    CHECK(mdr_strpbrk(text, "abc") == text + 5);
    CHECK(mdr_strpbrk(text, "xyz") == NULL);
}

static void test_strstr(void) {
    const char text[] = "the cat sat on the mat";

    CHECK(mdr_strstr(text, "cat") == text + 4);
    CHECK(mdr_strstr(text, "the") == text);
    CHECK(mdr_strstr(text, "the m") == text + 15);
    CHECK(mdr_strstr(text, "") == text);
    CHECK(mdr_strstr(text, "dog") == NULL);
    CHECK(mdr_strstr(text, "mat ") == NULL);
}

static void test_strtok(void) {
    char text[] = "  alpha,beta,,gamma  ";
    char *tok;

    tok = mdr_strtok(text, " ,");
    CHECK_STR_EQ(tok, "alpha");
    CHECK_STR_EQ(mdr_strtok(NULL, " ,"), "beta");
    CHECK_STR_EQ(mdr_strtok(NULL, " ,"), "gamma");
    CHECK(mdr_strtok(NULL, " ,") == NULL);
    CHECK(mdr_strtok(NULL, " ,") == NULL);
    CHECK_EQ_SIZE((unsigned char)text[7], 0u);

    {
        char empty[] = "   ";

        CHECK(mdr_strtok(empty, " ") == NULL);
    }
}

/* 与标准库对照：同一组输入分别交给 mdr_ 版本和 <string.h>，比较结果 */
struct find_case {
    const char *haystack;
    const char *needle;
};

static void test_against_libc(void) {
    static const struct find_case spans[] = {
        {"", ""},
        {"abc", ""},
        {"", "abc"},
        {"aabb", "ab"},
        {"12345abc", "0123456789"},
        {"12345abc", "abc"},
        {"hello world", " "},
    };
    static const struct find_case finds[] = {
        {"the cat sat on the mat", "cat"},
        {"the cat sat on the mat", "the"},
        {"the cat sat on the mat", "the m"},
        {"the cat sat on the mat", ""},
        {"the cat sat on the mat", "dog"},
        {"abc", "abcd"},
        {"aaaaab", "aab"},
    };

    for (size_t i = 0; i < sizeof spans / sizeof spans[0]; i++) {
        const char *h = spans[i].haystack;
        const char *n = spans[i].needle;

        CHECK_EQ_SIZE(mdr_strspn(h, n), strspn(h, n));
        CHECK_EQ_SIZE(mdr_strcspn(h, n), strcspn(h, n));
        CHECK(offset_or_neg(h, mdr_strpbrk(h, n)) ==
              offset_or_neg(h, strpbrk(h, n)));
    }
    for (size_t i = 0; i < sizeof finds / sizeof finds[0]; i++) {
        const char *h = finds[i].haystack;
        const char *n = finds[i].needle;

        CHECK(offset_or_neg(h, mdr_strstr(h, n)) == offset_or_neg(h, strstr(h, n)));
    }
}

int main(void) {
    test_strlen();
    test_strcmp();
    test_strnlen();
    test_strcpy_strcat();
    test_strncpy();
    test_strncmp();
    test_strchr();
    test_strspn();
    test_strstr();
    test_strtok();
    test_against_libc();
    return check_report("test_string");
}
