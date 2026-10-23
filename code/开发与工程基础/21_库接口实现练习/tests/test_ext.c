#include "check.h"
#include "mdr_ext.h"

#include <stdlib.h>

/* 21.5：扩展函数的单元测试。 */

static void test_strdup(void) {
    char *copy = mdr_strdup("hello");

    CHECK(copy != NULL);
    CHECK_STR_EQ(copy, "hello");
    CHECK(copy != NULL && copy != (const char *)"hello");
    free(copy);

    copy = mdr_strdup("");
    CHECK(copy != NULL);
    CHECK_STR_EQ(copy, "");
    free(copy);
}

static void test_strndup(void) {
    /* 不以空字符结尾的实参：只能读到 n 个字节为止 */
    const char raw[4] = {'a', 'b', 'c', 'd'};
    char *copy = mdr_strndup(raw, 4u);

    CHECK(copy != NULL);
    CHECK_EQ_SIZE(copy != NULL ? strlen(copy) : 0u, 4u);
    CHECK(copy != NULL && memcmp(copy, raw, 4u) == 0);
    free(copy);

    copy = mdr_strndup("hello", 2u);
    CHECK_STR_EQ(copy, "he");
    free(copy);

    copy = mdr_strndup("hi", 10u);
    CHECK_STR_EQ(copy, "hi");
    free(copy);

    copy = mdr_strndup("hello", 0u);
    CHECK_STR_EQ(copy, "");
    free(copy);
}

static void test_stpcpy(void) {
    char buf[8];
    char *end = mdr_stpcpy(buf, "abc");

    CHECK_STR_EQ(buf, "abc");
    CHECK(end == buf + 3);
    CHECK(*end == '\0');

    end = mdr_stpcpy(end, "de");
    CHECK_STR_EQ(buf, "abcde");
    CHECK(end == buf + 5);
}

static void test_strlcpy(void) {
    char buf[6];

    CHECK_EQ_SIZE(mdr_strlcpy(buf, "abc", sizeof buf), 3u);
    CHECK_STR_EQ(buf, "abc");
    CHECK_EQ_SIZE(mdr_strlcpy(buf, "abcdef", sizeof buf), 6u);
    CHECK_STR_EQ(buf, "abcde");
    CHECK_EQ_SIZE(mdr_strlcpy(buf, "abc", 0u), 3u);
    CHECK_EQ_SIZE(mdr_strlcpy(buf, "", sizeof buf), 0u);
    CHECK_STR_EQ(buf, "");
}

static void test_strlcat(void) {
    char buf[8];

    mdr_strlcpy(buf, "abc", sizeof buf);
    CHECK_EQ_SIZE(mdr_strlcat(buf, "de", sizeof buf), 5u);
    CHECK_STR_EQ(buf, "abcde");

    CHECK_EQ_SIZE(mdr_strlcat(buf, "xyz", sizeof buf), 8u);
    CHECK_STR_EQ(buf, "abcdexy");

    CHECK_EQ_SIZE(mdr_strlcat(buf, "", sizeof buf), 7u);
    CHECK_STR_EQ(buf, "abcdexy");

    /* dst 没有空字符时，strlcat 返回 dsize + strlen(src) */
    buf[0] = 'a';
    buf[1] = 'b';
    CHECK_EQ_SIZE(mdr_strlcat(buf, "zz", 2u), 4u);
}

static void test_memmem(void) {
    const char hay[] = {'a', 'b', 0, 'c', 'd', 'e'};
    const char needle[] = {0, 'c'};
    const char *p = (const char *)mdr_memmem(hay, sizeof hay, needle, sizeof needle);

    CHECK(p == hay + 2);
    CHECK(mdr_memmem(hay, sizeof hay, "cd", 2u) == hay + 3);
    CHECK(mdr_memmem(hay, sizeof hay, "ce", 2u) == NULL);
    CHECK(mdr_memmem(hay, sizeof hay, "", 0u) == hay);
    CHECK(mdr_memmem(hay, 2u, "abc", 3u) == NULL);
    CHECK(mdr_memmem(hay, sizeof hay, "ab", 2u) == hay);
    CHECK(mdr_memmem(hay, sizeof hay, "de", 2u) == hay + 4);
}

static void test_strtok_r(void) {
    char text[] = "  a,b,,c  ";
    char *save = NULL;
    char *tok = mdr_strtok_r(text, " ,", &save);

    CHECK_STR_EQ(tok, "a");
    CHECK_STR_EQ(mdr_strtok_r(NULL, " ,", &save), "b");
    CHECK_STR_EQ(mdr_strtok_r(NULL, " ,", &save), "c");
    CHECK(mdr_strtok_r(NULL, " ,", &save) == NULL);
    CHECK(mdr_strtok_r(NULL, " ,", &save) == NULL);
}

static void test_strtok_r_nested(void) {
    char outer[] = "a:b;c:d";
    char *save_outer = NULL;
    char *field = mdr_strtok_r(outer, ";", &save_outer);

    CHECK_STR_EQ(field, "a:b");
    while (field != NULL) {
        char *save_inner = NULL;
        char *sub = mdr_strtok_r(field, ":", &save_inner);
        while (sub != NULL) {
            CHECK(sub != NULL);
            sub = mdr_strtok_r(NULL, ":", &save_inner);
        }
        field = mdr_strtok_r(NULL, ";", &save_outer);
    }
}

static void test_strsep(void) {
    char text[] = "a,,b";
    char *rest = text;

    CHECK_STR_EQ(mdr_strsep(&rest, ","), "a");
    CHECK_STR_EQ(mdr_strsep(&rest, ","), "");
    CHECK_STR_EQ(mdr_strsep(&rest, ","), "b");
    CHECK(mdr_strsep(&rest, ",") == NULL);
    CHECK(rest == NULL);

    char empty[] = "";
    rest = empty;
    CHECK_STR_EQ(mdr_strsep(&rest, ","), "");
    CHECK(mdr_strsep(&rest, ",") == NULL);
}

static void test_ascii_casecmp(void) {
    CHECK(mdr_ascii_casecmp("Hello", "hello") == 0);
    CHECK(mdr_ascii_casecmp("ABC", "abc") == 0);
    CHECK(mdr_ascii_casecmp("", "") == 0);
    CHECK(mdr_ascii_casecmp("abc", "abd") < 0);
    CHECK(mdr_ascii_casecmp("abd", "abc") > 0);
    CHECK(mdr_ascii_casecmp("ab", "abc") < 0);
    CHECK(mdr_ascii_casecmp("B", "a") > 0);
    CHECK(mdr_ascii_casecmp("[", "a") < 0);
    CHECK(mdr_ascii_casecmp("@", "a") < 0);

    CHECK(mdr_ascii_ncasecmp("Hello", "help", 3u) == 0);
    CHECK(mdr_ascii_ncasecmp("Hello", "help", 4u) < 0);
    CHECK(mdr_ascii_ncasecmp("ABC", "abc", 0u) == 0);
}

int main(void) {
    test_strdup();
    test_strndup();
    test_stpcpy();
    test_strlcpy();
    test_strlcat();
    test_memmem();
    test_strtok_r();
    test_strtok_r_nested();
    test_strsep();
    test_ascii_casecmp();
    return check_report("test_ext");
}