/*
 * 第 16 章的事件 (a)：贡献者把 dup_string 换成了 strdup。
 * 在本机 MinGW（GCC 13.2，默认的 gnu17）上照常声明并链接；
 * 在 Linux 上用 -std=c17 严格模式构建时 glibc 不声明它，GCC 14 报错。
 *
 * 用它替换 src/counter.c 后构建：
 *   Linux:  cmake -S . -B build-fail && cmake --build build-fail
 *   MinGW:  同一份源码照常通过
 */
#include "wf/counter.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct item {
    char *word;
    size_t count;
};

struct wf_counter {
    struct item *items;
    size_t size;
    size_t capacity;
};

static struct item *find_entry(const wf_counter *c, const char *word) {
    for (size_t i = 0; i < c->size; i++) {
        struct item *e = &c->items[i];
        if (strcmp(word, e->word) == 0) {
            return e;
        }
    }
    return NULL;
}

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
    /* 缺陷：strdup 属于 POSIX，C17 严格模式下 glibc 不声明它 */
    char *copy = strdup(word);
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
