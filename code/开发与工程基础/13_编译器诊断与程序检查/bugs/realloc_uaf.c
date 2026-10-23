/*
 * clang-tidy 的 bugprone-suspicious-realloc-usage 示例：
 * 用 realloc 的返回值直接覆盖原指针，失败时原缓冲区泄漏。
 *
 *   clang-tidy realloc_uaf.c -checks='-*,bugprone-*,clang-analyzer-*' -- -std=c17
 *
 * wordfreq 的 reserve_one 先用临时指针接住 realloc 的返回值，就是为了避开这个问题。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct item {
    char *word;
    size_t count;
};

struct counter {
    struct item *items;
    size_t size;
    size_t capacity;
};

int add(struct counter *c, const char *word) {
    if (c->size == c->capacity) {
        size_t cap = c->capacity == 0 ? 4 : c->capacity * 2;
        /* 缺陷：realloc 失败时返回空指针，原来的 items 就找不回来了 */
        c->items = realloc(c->items, cap * sizeof *c->items);
        if (c->items == NULL) {
            return -1;
        }
        c->capacity = cap;
    }
    struct item *slot = &c->items[c->size];
    slot->word = malloc(strlen(word) + 1);
    if (slot->word == NULL) {
        return -1;
    }
    strcpy(slot->word, word);
    slot->count = 1;
    c->size++;
    return 0;
}

int main(void) {
    struct counter c = {0};
    for (int i = 0; i < 10; i++) {
        if (add(&c, "the") != 0) {
            return 1;
        }
    }
    printf("%zu\n", c.size);
    for (size_t i = 0; i < c.size; i++) {
        free(c.items[i].word);
    }
    free(c.items);
    return 0;
}
