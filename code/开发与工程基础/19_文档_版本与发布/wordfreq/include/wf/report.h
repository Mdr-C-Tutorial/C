/* SPDX-License-Identifier: MIT */
/**
 * @file report.h
 * @brief 把计数结果按固定格式写到输出流。
 */
#ifndef WF_REPORT_H
#define WF_REPORT_H

#include <stdio.h>

#include "wf/counter.h"
#include "wf/export.h"

/**
 * @brief 每项输出一行：次数右对齐占 7 列，一个空格，再接单词。
 * @param out 输出流
 * @param entries 计数结果数组
 * @param n 数组中的项数
 * @note 只读取 entries，不释放其中的 word
 */
WF_API void wf_report_print(FILE *out, const wf_entry *entries, size_t n);

#endif
