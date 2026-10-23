/* SPDX-License-Identifier: MIT */
/**
 * @file token.h
 * @brief 从一段文本中依次取出单词。
 *
 * 单词是连续的 ASCII 字母（A-Z、a-z），其他字节都是分隔符，
 * 取出的单词一律转成小写。
 */
#ifndef WF_TOKEN_H
#define WF_TOKEN_H

#include <stddef.h>

#include "wf/export.h"

/** @brief 单词缓冲区的长度，含终止空字符；超长的单词只保留前 63 个字母 */
#define WF_WORD_MAX 64

/** @brief 分词器的状态；调用方只通过下面两个函数使用，不直接读写成员 */
typedef struct {
    const char *cur; /**< 下一个待检查的字节 */
    const char *end; /**< 文本末尾之后的位置 */
} wf_tokenizer;

/**
 * @brief 准备从 text 的前 len 个字节中取单词。
 * @param[out] tz 分词器
 * @param text 文本的起始位置
 * @param len 文本的字节数
 * @note 取词期间 text 必须保持有效，分词器只保存指针
 */
WF_API void wf_tokenizer_init(wf_tokenizer *tz, const char *text, size_t len);

/**
 * @brief 取出下一个单词，转成小写写入 word，并在末尾放终止空字符。
 * @param tz 分词器
 * @param[out] word 接收单词的缓冲区
 * @param cap 缓冲区容量，必须大于 0；至多写入 cap-1 个字母
 * @return 写入的长度，0 表示没有更多单词
 */
WF_API size_t wf_tokenizer_next(wf_tokenizer *tz, char *word, size_t cap);

#endif
