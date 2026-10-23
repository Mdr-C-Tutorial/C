/* SPDX-License-Identifier: MIT */
/**
 * @file counter.h
 * @brief 统计每个单词出现的次数，并按次数取出前若干项。
 *
 * 计数器是一个不透明类型：结构定义在 src/counter.c 中，调用方只能通过
 * 下面的函数使用它。create 与 destroy 成对调用。
 */
#ifndef WF_COUNTER_H
#define WF_COUNTER_H

#include <stddef.h>

#include "wf/export.h"

/** @brief 计数器；定义在实现文件中，调用方不直接读写成员 */
typedef struct wf_counter wf_counter;

/** @brief 一项计数结果 */
typedef struct {
    /** 指向计数器内部存储，在下一次 wf_counter_add 或
        wf_counter_destroy 之前有效；调用方不要 free */
    const char *word;
    /** 该单词出现的次数 */
    size_t count;
} wf_entry;

/**
 * @brief 创建空计数器。
 * @return 新计数器；内存不足时返回 NULL
 */
WF_API wf_counter *wf_counter_create(void);

/**
 * @brief 释放计数器以及它保存的全部单词。
 * @param c 计数器，可以为 NULL
 */
WF_API void wf_counter_destroy(wf_counter *c);

/**
 * @brief 把 word 的次数加 1，第一次出现时保存一份副本。
 * @param c 计数器
 * @param word 以空字符结尾的单词，函数内部复制，调用后调用方可以释放
 * @return 0 成功，-1 内存不足
 */
WF_API int wf_counter_add(wf_counter *c, const char *word);

/**
 * @brief 返回 word 出现的次数。
 * @param c 计数器
 * @param word 要查询的单词
 * @return 次数；没有出现过时返回 0
 */
WF_API size_t wf_counter_get(const wf_counter *c, const char *word);

/**
 * @brief 返回不同单词的个数。
 * @param c 计数器
 * @return 不同单词的个数
 */
WF_API size_t wf_counter_size(const wf_counter *c);

/**
 * @brief 按次数降序、次数相同按 strcmp 升序取出前 n 项。
 * @param c 计数器
 * @param[out] out 接收结果的数组，至少能放 n 项
 * @param n 请求的项数
 * @return 写入的项数 min(n, wf_counter_size(c))；内存不足时返回 0
 * @note 结果中的 word 指针在下次 wf_counter_add 之前有效
 */
WF_API size_t wf_counter_top(const wf_counter *c, wf_entry *out, size_t n);

#endif
