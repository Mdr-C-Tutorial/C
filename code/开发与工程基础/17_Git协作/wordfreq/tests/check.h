/*
 * 一个很小的测试框架：CHECK 系列宏记录一次检查的结果，
 * check_report 在 main 末尾打印汇总并给出退出状态。
 */
#ifndef CHECK_H
#define CHECK_H

#include <stddef.h>
#include <string.h>

void check_pass(void);
void check_fail(const char *file, int line, const char *fmt, ...);

/* 打印「<suite>: <总数> checks, <失败数> failed」；有失败返回 1，否则返回 0 */
int check_report(const char *suite);

/* 失败信息为「文件:行: 条件原文」 */
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (cond) {                                                            \
            check_pass();                                                      \
        } else {                                                               \
            check_fail(__FILE__, __LINE__, "%s", #cond);                       \
        }                                                                      \
    } while (0)

/* 比较两个 size_t；失败信息为「表达式 is 实际值, expected 期望值」 */
#define CHECK_EQ_SIZE(actual, expected)                                        \
    do {                                                                       \
        size_t check_actual_ = (actual);                                       \
        size_t check_expected_ = (expected);                                   \
        if (check_actual_ == check_expected_) {                                \
            check_pass();                                                      \
        } else {                                                               \
            check_fail(__FILE__, __LINE__, "%s is %zu, expected %zu",          \
                       #actual, check_actual_, check_expected_);               \
        }                                                                      \
    } while (0)

/* 比较两个字符串；失败信息形如 word is "The", expected "the" */
#define CHECK_STR_EQ(actual, expected)                                         \
    do {                                                                       \
        const char *check_actual_ = (actual);                                  \
        const char *check_expected_ = (expected);                              \
        if (check_actual_ != NULL && check_expected_ != NULL &&                \
            strcmp(check_actual_, check_expected_) == 0) {                     \
            check_pass();                                                      \
        } else {                                                               \
            check_fail(__FILE__, __LINE__, "%s is \"%s\", expected \"%s\"",    \
                       #actual, check_actual_ != NULL ? check_actual_ : "(null)", \
                       check_expected_ != NULL ? check_expected_ : "(null)");  \
        }                                                                      \
    } while (0)

#endif
