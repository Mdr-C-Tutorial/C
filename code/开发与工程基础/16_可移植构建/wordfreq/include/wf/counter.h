/*
 * 计数模块的接口：记录每个单词出现的次数，并按次数取出前若干项。
 */
#ifndef WF_COUNTER_H
#define WF_COUNTER_H

#include <stddef.h>

#include "wf/export.h"

/* 结构定义在 src/counter.c 中，调用方只能通过指针使用计数器 */
typedef struct wf_counter wf_counter;

typedef struct {
    /* 指向计数器内部存储，在下一次 wf_counter_add 或
       wf_counter_destroy 之前有效 */
    const char *word;
    size_t count;
} wf_entry;

/* 创建空计数器；内存不足时返回空指针 */
WF_API wf_counter *wf_counter_create(void);

/* 释放计数器以及它保存的全部单词 */
WF_API void wf_counter_destroy(wf_counter *c);

/* 把 word 的次数加 1，第一次出现时保存一份副本；0 成功，-1 内存不足 */
WF_API int wf_counter_add(wf_counter *c, const char *word);

/* 返回 word 出现的次数，没有出现过时返回 0 */
WF_API size_t wf_counter_get(const wf_counter *c, const char *word);

/* 返回不同单词的个数 */
WF_API size_t wf_counter_size(const wf_counter *c);

/*
 * 按次数降序、次数相同按 strcmp 升序取出前 n 项。
 * 返回写入的项数 min(n, size)；内存不足时返回 0。
 */
WF_API size_t wf_counter_top(const wf_counter *c, wf_entry *out, size_t n);

#endif
