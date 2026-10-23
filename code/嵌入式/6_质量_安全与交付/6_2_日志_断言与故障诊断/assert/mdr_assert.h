/*
 * mdr_assert.h：嵌入式断言宏。
 *
 * 断言失败时把文件、行号、返回地址交给 diag_assert_failed()，由平台实现
 * 决定是断点、记录后复位还是停机。产品固件里保留断言。
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#ifndef MDR_ASSERT_H
#define MDR_ASSERT_H

#include <stdint.h>

/* 断言处理函数：file、line 是断言失败的源码位置，ret 是调用点的返回地址 */
void diag_assert_failed(const char *file, int line, uintptr_t ret);

#define MDR_ASSERT(cond)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            diag_assert_failed(__FILE__, __LINE__,                           \
                               (uintptr_t)__builtin_return_address(0));      \
        }                                                                    \
    } while (0)

#endif /* MDR_ASSERT_H */