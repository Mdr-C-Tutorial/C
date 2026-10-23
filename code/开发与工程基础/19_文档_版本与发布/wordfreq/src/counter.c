/* SPDX-License-Identifier: MIT */
/*
 * v0.2：条目按单词升序存放，查找用二分查找（lower_bound），
 * 插入新单词时用 memmove 把后面的元素整体后移。
 *
 * 接口与 v0.1 完全相同：调用方看不到这里的数组有序，
 * wf_counter_top 的结果仍然是按次数降序、同次数按 strcmp 升序。
 */
#include "wf/counter.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

/* 在有序数组中找 word 应当插入的位置：第一个不小于 word 的元素下标 */
static size_t lower_bound(const wf_counter *c, const char *word) {
    size_t lo = 0;
    size_t hi = c->size;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (strcmp(c->items[mid].word, word) < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
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
    size_t pos = lower_bound(c, word);
    if (pos < c->size && strcmp(c->items[pos].word, word) == 0) {
        c->items[pos].count++;
        return 0;
    }

    if (reserve_one(c) != 0) {
        return -1;
    }
    char *copy = dup_string(word);
    if (copy == NULL) {
        return -1;
    }
    memmove(&c->items[pos + 1], &c->items[pos],
            (c->size - pos) * sizeof c->items[0]);
    c->items[pos].word = copy;
    c->items[pos].count = 1;
    c->size++;
    return 0;
}

size_t wf_counter_get(const wf_counter *c, const char *word) {
    size_t pos = lower_bound(c, word);
    if (pos < c->size && strcmp(c->items[pos].word, word) == 0) {
        return c->items[pos].count;
    }
    return 0;
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
