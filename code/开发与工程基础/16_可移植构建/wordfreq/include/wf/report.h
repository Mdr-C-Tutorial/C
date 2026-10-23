/*
 * 输出模块的接口：把计数结果按固定格式写到输出流。
 */
#ifndef WF_REPORT_H
#define WF_REPORT_H

#include <stdio.h>

#include "wf/counter.h"
#include "wf/export.h"

/* 每项输出一行：次数右对齐占 7 列，一个空格，再接单词 */
WF_API void wf_report_print(FILE *out, const wf_entry *entries, size_t n);

#endif
