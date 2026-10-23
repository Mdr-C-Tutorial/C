/*
 * 第 14 章的模糊测试目标：把输入当作一段文本，按第一个字节决定单词缓冲区的
 * 大小，其余字节交给分词器。
 *
 * 检查的性质：
 *   - 返回的长度小于 cap；
 *   - 写入的单词只含小写字母；
 *   - 终止空字符在 word[len] 处。
 *
 * 缓冲区按 cap 精确分配，ASan 能看到 word[cap] 的越界写。
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "wf/token.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) {
        return 0;
    }

    size_t cap = (size_t)(data[0] % WF_WORD_MAX) + 1;
    const char *text = (const char *)data + 1;
    size_t len = size - 1;

    char *word = malloc(cap);
    if (word == NULL) {
        return 0;
    }

    wf_tokenizer tz;
    wf_tokenizer_init(&tz, text, len);
    size_t n = wf_tokenizer_next(&tz, word, cap);
    if (n >= cap) {
        abort();
    }
    for (size_t i = 0; i < n; i++) {
        if (word[i] < 'a' || word[i] > 'z') {
            abort();
        }
    }
    if (word[n] != '\0') {
        abort();
    }

    free(word);
    return 0;
}
