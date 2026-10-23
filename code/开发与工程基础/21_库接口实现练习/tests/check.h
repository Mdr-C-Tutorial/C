#ifndef CHECK_H
#define CHECK_H

#include <stddef.h>
#include <string.h>

void check_pass(void);
void check_fail(const char *file, int line, const char *fmt, ...);
int check_report(const char *suite);

#define CHECK(cond)                                      \
    do {                                                 \
        if (cond) {                                      \
            check_pass();                                \
        } else {                                         \
            check_fail(__FILE__, __LINE__, "%s", #cond); \
        }                                                \
    } while (0)

#define CHECK_EQ_SIZE(actual, expected)                                        \
    do {                                                                       \
        size_t check_act_ = (size_t)(actual);                                  \
        size_t check_exp_ = (size_t)(expected);                                \
        if (check_act_ == check_exp_) {                                        \
            check_pass();                                                      \
        } else {                                                               \
            check_fail(__FILE__, __LINE__, "%s is %zu, expected %zu", #actual, \
                       check_act_, check_exp_);                                \
        }                                                                      \
    } while (0)

#define CHECK_STR_EQ(actual, expected)                                         \
    do {                                                                       \
        const char *check_act_ = (actual);                                     \
        const char *check_exp_ = (expected);                                   \
        if (check_act_ != NULL && check_exp_ != NULL &&                        \
            strcmp(check_act_, check_exp_) == 0) {                             \
            check_pass();                                                      \
        } else {                                                               \
            check_fail(__FILE__, __LINE__, "%s is \"%s\", expected \"%s\"",    \
                       #actual, check_act_ ? check_act_ : "(null)",            \
                       check_exp_ ? check_exp_ : "(null)");                    \
        }                                                                      \
    } while (0)

#endif /* CHECK_H */
