/*
 * 统计文件的接口：以空白分隔的连续字符算一个单词。
 */
#ifndef LINES_H
#define LINES_H

#include <stddef.h>

/* 返回 path 中单词的个数；读文件失败时返回 0 */
size_t count_words_in_file(const char *path);

#endif
