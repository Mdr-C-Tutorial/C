#ifndef MDR_COMPILER_H
#define MDR_COMPILER_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * 调试构建中的前置条件检查：定义了 NDEBUG 时展开为空操作。
 * 只用于检查调用方违反前置条件这类程序错误。
 */
#define MDR_ASSERT(cond) assert(cond)

/*
 * 21.6：编译器能力检测与可回退的宏。
 * 定义了 MDR_NO_BUILTINS 时全部走普通 C 回退，用于测试两条路径结果一致。
 */
#if defined(MDR_NO_BUILTINS)
#define MDR_USE_BUILTINS 0
#else
#define MDR_USE_BUILTINS 1
#endif

/* GCC 10 起和 Clang 提供 __has_builtin；其他编译器按 0 处理。 */
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#define MDR_HAS_BUILTIN(x) __has_builtin(x)

/* C23 <stdbit.h>、<stdckdint.h>：用 __has_include 检测，不看版本号。 */
#if MDR_USE_BUILTINS && defined(__has_include)
#if __has_include(<stdbit.h>)
#include <stdbit.h>
#define MDR_HAVE_STDBIT 1
#endif
#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#define MDR_HAVE_STDCKDINT 1
#endif
#endif
#ifndef MDR_HAVE_STDBIT
#define MDR_HAVE_STDBIT 0
#endif
#ifndef MDR_HAVE_STDCKDINT
#define MDR_HAVE_STDCKDINT 0
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

/* 带溢出检查的 size_t 加法：溢出返回 true，否则把和写入 *res 并返回 false。 */
#if MDR_HAVE_STDCKDINT
#define MDR_CKD_ADD_SIZE(a, b, res) (ckd_add((res), (a), (b)))
#elif MDR_USE_BUILTINS && MDR_HAS_BUILTIN(__builtin_add_overflow)
#define MDR_CKD_ADD_SIZE(a, b, res) __builtin_add_overflow((a), (b), (res))
#else
#define MDR_CKD_ADD_SIZE(a, b, res)                                            \
    ((a) > SIZE_MAX - (b) ? true : (*(res) = (a) + (b), false))
#endif

/* 分支提示。回退时只保留表达式本身，括号保持求值次数不变。 */
#if MDR_USE_BUILTINS && (defined(__GNUC__) || defined(__clang__))
#define MDR_LIKELY(x) __builtin_expect(!!(x), 1)
#define MDR_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define MDR_UNREACHABLE() __builtin_unreachable()
#elif MDR_USE_BUILTINS && defined(_MSC_VER)
#define MDR_LIKELY(x) (x)
#define MDR_UNLIKELY(x) (x)
#define MDR_UNREACHABLE() __assume(0)
#else
#define MDR_LIKELY(x) (x)
#define MDR_UNLIKELY(x) (x)
#define MDR_UNREACHABLE() abort()
#endif

/* 属性的回退：没有属性时展开为空，代码照常编译。
 * -DMDR_NO_ATTRIBUTES 在支持属性的编译器上也能强制走空展开，便于检查两种写法。 */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(MDR_NO_ATTRIBUTES)
#define MDR_NODISCARD __attribute__((warn_unused_result))
#define MDR_MAYBE_UNUSED __attribute__((unused))
#define MDR_FALLTHROUGH __attribute__((fallthrough))
#define MDR_PRINTF_LIKE(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define MDR_NODISCARD
#define MDR_MAYBE_UNUSED
#define MDR_FALLTHROUGH
#define MDR_PRINTF_LIKE(fmt, args)
#endif

#endif /* MDR_COMPILER_H */