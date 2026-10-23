/*
 * test.h：极简的测试框架。
 *
 * CHECK 失败时打印文件名、行号和条件，计数后继续执行，一次运行就能
 * 看到全部失败。每个测试文件是一个独立的程序，返回值 0 表示全部通过。
 */
#ifndef MDRDB_TEST_H
#define MDRDB_TEST_H

#include "mdrdb.h"

#include <stdio.h>

static int test_checks;
static int test_failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        test_checks++;                                                         \
        if (!(cond)) {                                                         \
            test_failures++;                                                   \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,   \
                    #cond);                                                    \
        }                                                                      \
    } while (0)

#define CHECK_STATUS(expr, expected)                                           \
    do {                                                                       \
        db_status got_ = (expr);                                               \
        test_checks++;                                                         \
        if (got_ != (expected)) {                                              \
            test_failures++;                                                   \
            fprintf(stderr, "%s:%d: %s returned \"%s\", expected \"%s\"\n",    \
                    __FILE__, __LINE__, #expr, db_status_message(got_),        \
                    db_status_message(expected));                              \
        }                                                                      \
    } while (0)

#define CHECK_OK(expr) CHECK_STATUS(expr, DB_OK)

#define RUN_TEST(fn)                                                           \
    do {                                                                       \
        int before_ = test_failures;                                           \
        fn();                                                                  \
        printf("%-4s %s\n", test_failures == before_ ? "ok" : "FAIL", #fn);    \
    } while (0)

static inline int test_summary(void) {
    printf("%d checks, %d failed\n", test_checks, test_failures);
    return test_failures == 0 ? 0 : 1;
}

/* 删除数据库文件和它的日志。文件不存在时 remove 失败，忽略即可。 */
static inline void remove_db_files(const char *path) {
    char journal_path[256];
    snprintf(journal_path, sizeof journal_path, "%s-journal", path);
    remove(path);
    remove(journal_path);
}

#endif
