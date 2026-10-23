#ifndef CHECK_H
#define CHECK_H

/* 极简的测试断言：失败时打印位置和表达式，计数后继续执行后面的检查，
 * 这样一次运行就能看到所有失败的地方。 */

#include <stdio.h>

static int check_total = 0;
static int check_failed = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        check_total++;                                                         \
        if (!(cond)) {                                                         \
            check_failed++;                                                    \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,   \
                    #cond);                                                    \
        }                                                                      \
    } while (0)

/* 打印汇总，返回值直接作为 main 的返回值：0 表示全部通过。 */
static int check_report(const char *suite) {
    printf("%s: %d checks, %d failed\n", suite, check_total, check_failed);
    return check_failed == 0 ? 0 : 1;
}

#endif
