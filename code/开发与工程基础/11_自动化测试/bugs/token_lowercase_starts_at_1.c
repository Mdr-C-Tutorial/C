/*
 * 第 11 章的注入缺陷：小写转换的循环从下标 1 开始，下标 0 的字母不转换。
 * 用它替换 src/token.c，tests/test_token.c 的 test_lowercase_folding 会失败。
 */
#include "wf/token.h"

/* 只认 ASCII 字母：不用 <ctype.h>，结果不随区域设置变化 */
static int is_ascii_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

void wf_tokenizer_init(wf_tokenizer *tz, const char *text, size_t len) {
    tz->cur = text;
    tz->end = text + len;
}

size_t wf_tokenizer_next(wf_tokenizer *tz, char *word, size_t cap) {
    if (cap == 0) {
        return 0;
    }

    const char *p = tz->cur;
    while (p < tz->end && !is_ascii_letter(*p)) {
        p++;
    }

    /* 放不下的字母读过就丢弃，不会成为下一个单词的开头 */
    size_t n = 0;
    while (p < tz->end && is_ascii_letter(*p)) {
        if (n + 1 < cap) {
            word[n++] = *p;
        }
        p++;
    }
    word[n] = '\0';

    /* 缺陷：从 1 开始转换，word[0] 保持原样 */
    for (size_t i = 1; i < n; i++) {
        if (word[i] >= 'A' && word[i] <= 'Z') {
            word[i] = (char)(word[i] - 'A' + 'a');
        }
    }

    tz->cur = p;
    return n;
}
