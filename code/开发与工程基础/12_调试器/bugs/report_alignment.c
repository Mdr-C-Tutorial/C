/*
 * 第 12 章缺陷 (a) 的前一半：report.c 改成「按最长单词对齐」，
 * 逐字符遍历 entries[i].word 求最长单词的宽度。单独使用没有问题，
 * 与 main_passes_n.c 一起用时，超出实际项数的条目 word 为空指针，这里会崩溃。
 */
#include "wf/report.h"

void wf_report_print(FILE *out, const wf_entry *entries, size_t n) {
    size_t width = 0;
    for (size_t i = 0; i < n; i++) {
        size_t len = 0;
        for (const char *p = entries[i].word; *p != '\0'; p++) {
            len++;
        }
        if (len > width) {
            width = len;
        }
    }

    for (size_t i = 0; i < n; i++) {
        fprintf(out, "%7zu %-*s\n", entries[i].count, (int)width,
                entries[i].word);
    }
}
