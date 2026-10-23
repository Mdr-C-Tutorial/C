/* SPDX-License-Identifier: MIT */
#include "wf/token.h"

/* 只认 ASCII 字母：不用 <ctype.h>，结果不随区域设置变化 */
static int is_ascii_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static char to_ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
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
            word[n++] = to_ascii_lower(*p);
        }
        p++;
    }
    word[n] = '\0';

    tz->cur = p;
    return n;
}
