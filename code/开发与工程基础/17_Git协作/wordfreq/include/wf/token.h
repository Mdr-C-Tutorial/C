/*
 * 分词模块的接口：从一段文本中依次取出单词。
 *
 * 单词是连续的 ASCII 字母（A-Z、a-z），其它字节都是分隔符，
 * 取出的单词一律转成小写。
 */
#ifndef WF_TOKEN_H
#define WF_TOKEN_H

#include <stddef.h>

#include "wf/export.h"

/* 单词缓冲区的长度，含终止空字符；超长的单词只保留前 63 个字母 */
#define WF_WORD_MAX 64

/* 分词器的状态；调用方只通过下面两个函数使用，不直接读写成员 */
typedef struct {
    const char *cur;
    const char *end;
} wf_tokenizer;

/* 准备从 text 的前 len 个字节中取单词；取词期间 text 必须保持有效 */
WF_API void wf_tokenizer_init(wf_tokenizer *tz, const char *text, size_t len);

/*
 * 取出下一个单词，转成小写写入 word，并在末尾放终止空字符。
 * cap >= 1：至多写入 cap-1 个字母；
 * 返回写入的长度，0 表示没有更多单词。
 */
WF_API size_t wf_tokenizer_next(wf_tokenizer *tz, char *word, size_t cap);

#endif
