#include "wf/counter.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * v0.1：单词按第一次出现的顺序存放在动态数组中，
 * 每加入一个单词都从头线性查找。
 */

struct item {
    char *word; /* 计数器自己保存的副本 */
    size_t count;
};

struct wf_counter {
    struct item *items;
    size_t size;     /* 已使用的元素个数 */
    size_t capacity; /* items 能容纳的元素个数 */
};

/* 复制字符串；内存不足时返回空指针 */
static char *dup_string(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

/* 线性查找 word；找不到时返回空指针 */
static struct item *find_entry(const wf_counter *c, const char *word) {
    for (size_t i = 0; i < c->size; i++) {
        struct item *e = &c->items[i];
        if (strcmp(word, e->word) == 0) {
            return e;
        }
    }
    return NULL;
}

/* 保证 items 还能再放一个元素：容量从 16 开始，不够时翻倍 */
static int reserve_one(wf_counter *c) {
    if (c->size < c->capacity) {
        return 0;
    }
    size_t capacity = c->capacity == 0 ? 16 : c->capacity * 2;
    if (capacity > SIZE_MAX / sizeof(struct item)) {
        return -1;
    }
    struct item *items = realloc(c->items, capacity * sizeof(struct item));
    if (items == NULL) {
        return -1;
    }
    c->items = items;
    c->capacity = capacity;
    return 0;
}

wf_counter *wf_counter_create(void) {
    wf_counter *c = malloc(sizeof *c);
    if (c == NULL) {
        return NULL;
    }
    c->items = NULL;
    c->size = 0;
    c->capacity = 0;
    return c;
}

void wf_counter_destroy(wf_counter *c) {
    if (c == NULL) {
        return;
    }
    for (size_t i = 0; i < c->size; i++) {
        free(c->items[i].word);
    }
    free(c->items);
    free(c);
}

int wf_counter_add(wf_counter *c, const char *word) {
    struct item *e = find_entry(c, word);
    if (e != NULL) {
        e->count++;
        return 0;
    }

    if (reserve_one(c) != 0) {
        return -1;
    }
    char *copy = dup_string(word);
    if (copy == NULL) {
        return -1;
    }
    c->items[c->size].word = copy;
    c->items[c->size].count = 1;
    c->size++;
    return 0;
}

size_t wf_counter_get(const wf_counter *c, const char *word) {
    const struct item *e = find_entry(c, word);
    return e != NULL ? e->count : 0;
}

size_t wf_counter_size(const wf_counter *c) { return c->size; }

/* qsort 的比较函数：次数多的在前，次数相同按 strcmp 升序 */
static int compare_entries(const void *a, const void *b) {
    const wf_entry *x = a;
    const wf_entry *y = b;
    if (x->count != y->count) {
        return x->count > y->count ? -1 : 1;
    }
    return strcmp(x->word, y->word);
}

size_t wf_counter_top(const wf_counter *c, wf_entry *out, size_t n) {
    size_t m = n < c->size ? n : c->size;
    if (m == 0) {
        return 0;
    }

    /* 把全部条目复制到临时数组排序，计数器本身的顺序不变 */
    wf_entry *all = malloc(c->size * sizeof *all);
    if (all == NULL) {
        return 0;
    }
    for (size_t i = 0; i < c->size; i++) {
        all[i].word = c->items[i].word;
        all[i].count = c->items[i].count;
    }
    qsort(all, c->size, sizeof *all, compare_entries);
    memcpy(out, all, m * sizeof *out);
    free(all);
    return m;
}
