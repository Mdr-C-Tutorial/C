/*
 * test_assert.h：与 Unity 同名同形的最小断言集合。
 *
 * 本教程的示例不把第三方测试框架提交进仓库。这个头文件只实现页面上用到的
 * 几个宏，接口与 Unity v2.7.0 相同；换成 Unity 时把 unity.c 链接进来，
 * 测试代码一行都不用改。
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H

#include <stdint.h>
#include <stdio.h>

static int g_test_failures;
static int g_test_checks;
static const char *g_test_current;

#define TEST_ASSERT_EQUAL_INT(expected, actual)                              \
    do {                                                                     \
        g_test_checks++;                                                     \
        long e_ = (long)(expected);                                          \
        long a_ = (long)(actual);                                            \
        if (e_ != a_) {                                                      \
            printf("FAIL %s:%d %s: expected %ld, got %ld\n", __FILE__,       \
                   __LINE__, g_test_current, e_, a_);                         \
            g_test_failures++;                                               \
        }                                                                    \
    } while (0)

#define TEST_ASSERT_EQUAL_HEX8(expected, actual)                             \
    do {                                                                     \
        g_test_checks++;                                                     \
        unsigned e_ = (unsigned)(expected) & 0xFFu;                          \
        unsigned a_ = (unsigned)(actual) & 0xFFu;                            \
        if (e_ != a_) {                                                      \
            printf("FAIL %s:%d %s: expected 0x%02X, got 0x%02X\n", __FILE__, \
                   __LINE__, g_test_current, e_, a_);                          \
            g_test_failures++;                                               \
        }                                                                    \
    } while (0)

#define TEST_ASSERT_EQUAL_HEX16(expected, actual)                            \
    do {                                                                     \
        g_test_checks++;                                                     \
        unsigned e_ = (unsigned)(expected) & 0xFFFFu;                        \
        unsigned a_ = (unsigned)(actual) & 0xFFFFu;                          \
        if (e_ != a_) {                                                      \
            printf("FAIL %s:%d %s: expected 0x%04X, got 0x%04X\n", __FILE__, \
                   __LINE__, g_test_current, e_, a_);                          \
            g_test_failures++;                                               \
        }                                                                    \
    } while (0)

#define TEST_ASSERT_TRUE(cond)                                               \
    do {                                                                     \
        g_test_checks++;                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d %s: expected true\n", __FILE__, __LINE__,     \
                   g_test_current);                                          \
            g_test_failures++;                                               \
        }                                                                    \
    } while (0)

#define TEST_ASSERT_FALSE(cond) TEST_ASSERT_TRUE(!(cond))

#define TEST_ASSERT_NOT_NULL(ptr) TEST_ASSERT_TRUE((ptr) != NULL)

/* 与 Unity 的运行器同名：UNITY_BEGIN、RUN_TEST、UNITY_END */
#define UNITY_BEGIN()                                                            do {                                                                             g_test_failures = 0;                                                         g_test_checks = 0;                                                       } while (0)

#define RUN_TEST(fn)                                                         \
    do {                                                                     \
        g_test_current = #fn;                                                \
        printf("RUN  %s\n", #fn);                                            \
        setUp();                                                             \
        fn();                                                                \
        tearDown();                                                          \
    } while (0)

#define UNITY_END()                                                          \
    (printf("%d checks, %d failures\n", g_test_checks, g_test_failures),     \
     g_test_failures == 0 ? 0 : 1)

#endif /* TEST_ASSERT_H */