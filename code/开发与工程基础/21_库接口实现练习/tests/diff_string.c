#include "diff_util.h"
#include "mdr_string.h"

#include <stdio.h>
#include <string.h>

/* 在字母表字母上按字典序生成下一个等长字符串；全部回绕时返回 0。 */
static int next_string(char *buf, size_t len, const char *alphabet) {
    size_t alpha_len = strlen(alphabet);

    for (size_t i = len; i > 0; i--) {
        const char *pos = strchr(alphabet, buf[i - 1]);

        if ((size_t)(pos - alphabet) + 1 < alpha_len) {
            buf[i - 1] = pos[1];
            return 1;
        }
        buf[i - 1] = alphabet[0];
    }
    return 0;
}

/* 对一对 (haystack, needle) 比较六个查找函数，结果统一换成偏移或长度。 */
static void compare_one(const char *haystack, const char *needle) {
    size_t hay_len = strlen(haystack);
    long want;
    long got;

    want = diff_offset(haystack, strchr(haystack, 'a'));
    got = diff_offset(haystack, mdr_strchr(haystack, 'a'));
    if (want != got) {
        diff_report("strchr('a')", haystack, hay_len, want, got);
    } else {
        diff_ok();
    }

    want = diff_offset(haystack, strrchr(haystack, 'b'));
    got = diff_offset(haystack, mdr_strrchr(haystack, 'b'));
    if (want != got) {
        diff_report("strrchr('b')", haystack, hay_len, want, got);
    } else {
        diff_ok();
    }

    want = (long)strspn(haystack, needle);
    got = (long)mdr_strspn(haystack, needle);
    if (want != got) {
        diff_report("strspn", haystack, hay_len, want, got);
    } else {
        diff_ok();
    }

    want = (long)strcspn(haystack, needle);
    got = (long)mdr_strcspn(haystack, needle);
    if (want != got) {
        diff_report("strcspn", haystack, hay_len, want, got);
    } else {
        diff_ok();
    }

    want = diff_offset(haystack, strpbrk(haystack, needle));
    got = diff_offset(haystack, mdr_strpbrk(haystack, needle));
    if (want != got) {
        diff_report("strpbrk", haystack, hay_len, want, got);
    } else {
        diff_ok();
    }

    want = diff_offset(haystack, strstr(haystack, needle));
    got = diff_offset(haystack, mdr_strstr(haystack, needle));
    if (want != got) {
        diff_report("strstr", haystack, hay_len, want, got);
    } else {
        diff_ok();
    }
}

/* 穷举：haystack 长度 0–4，needle 长度 0–2，字母表 {'a','b','c'}。 */
static void exhaustive_pairs(void) {
    static const char alphabet[] = "abc";
    char haystack[5];
    char needle[3];

    for (size_t hlen = 0; hlen <= 4; hlen++) {
        haystack[hlen] = '\0';
        for (size_t i = 0; i < hlen; i++) {
            haystack[i] = alphabet[0];
        }
        do {
            for (size_t nlen = 0; nlen <= 2; nlen++) {
                needle[nlen] = '\0';
                for (size_t i = 0; i < nlen; i++) {
                    needle[i] = alphabet[0];
                }
                do {
                    compare_one(haystack, needle);
                } while (next_string(needle, nlen, alphabet));
            }
        } while (next_string(haystack, hlen, alphabet));
    }
}

/* 单个字节的全部 256 个取值再扫一遍：小字母表到不了 0x80 以上。 */
static void every_byte(void) {
    char text[2] = {0, 0};

    for (int c = 0; c < 256; c++) {
        text[0] = (char)c;
        compare_one(text, "abc");
        compare_one(text, "a");
    }
}

int main(void) {
    exhaustive_pairs();
    every_byte();
    return diff_summary("diff_string");
}