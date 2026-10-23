/*
 * 第 14 章的注入缺陷：为 cap 为 1 的调用加了一条「小缓冲区」捷径，
 * 复制单词时漏掉了 n + 1 < cap 的检查，终止空字符写到了缓冲区之外。
 *
 * 单元测试从没用 cap 为 1 调用过 wf_tokenizer_next，gcov 报告里这一整块
 * 显示 #####，所以 27 项检查全部通过；fuzz/fuzz_token.c 用输入的第一个字节
 * 决定 cap，配合 AddressSanitizer 在 2 个字节的输入上就能发现它。
 */
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

    /* 缺陷：cap 为 1 时不做截断检查，单词有几个字母就写几个字节 */
    if (cap == 1) {
        size_t n = 0;
        while (p < tz->end && is_ascii_letter(*p)) {
            word[n++] = to_ascii_lower(*p);
            p++;
        }
        word[n] = '\0';
        tz->cur = p;
        return n;
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
