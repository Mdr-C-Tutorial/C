# 带检查的整数算术

在进行整数运算时，一个常见但又经常被忽略的问题是**整数溢出**。当算术运算的结果超出了其数据类型所能表示的范围时，就会发生溢出。在 C 语言中，有符号整数的溢出是未定义行为（Undefined Behavior, UB），这意味着编译器可以做任何事情——从产生一个回绕（wrap-around）的值，到让你的程序崩溃。这种不确定性是许多安全漏洞（如整数溢出漏洞）的根源。

为了解决这个问题，C23 标准引入了一个新的头文件 `<stdckdint.h>`，提供了一组函数来执行带检查的整数算术。这些函数可以安全地告诉你一次运算是否会导致溢出，从而让你能够优雅地处理这些情况，而不是触发未定义行为。

::: warning 注意

`<stdckdint.h>` 属于 C23 新增内容。如果你的实现没有该头文件，可以把这一节当作“了解 C23 新能力”；在工程中则需要采用实现扩展或手写检查逻辑。

:::

## C23 之前的解决方案

在 C23 之前，C 语言程序员通常依赖于编译器特定的内建函数或者手动编写检查代码来检测整数溢出。例如，GCC 和 Clang 提供了一些内建函数，如 `__builtin_add_overflow`。

```c
#include <stdio.h>

int main(void) {
    int a = 1000000000;
    int b = 2000000000;
    int result;
    if (__builtin_add_overflow(a, b, &result)) {
        printf("发生溢出！\n");
    } else {
        printf("结果是：%d\n", result);
    }
    return 0;
}
```

虽然这些内建函数很有用，但它们不是标准 C 的一部分，这意味着代码的可移植性会受到影响。

## C23 `<stdckdint.h>`

C23 标准通过引入 `<stdckdint.h>` 头文件，将带检查的整数算术标准化。这个头文件定义了三个函数，用于检查加法、减法和乘法运算是否会溢出。

- `ckd_add(result, a, b)`: 检查 `a + b` 是否溢出。
- `ckd_sub(result, a, b)`: 检查 `a - b` 是否溢出。
- `ckd_mul(result, a, b)`: 检查 `a * b` 是否溢出。

### 函数原型

这些函数都遵循相似的模式。我们以 `ckd_add` 为例：

```c
bool ckd_add(type *result, type1 a, type2 b);
```

- `result`: 一个指向结果存储位置的指针。如果运算没有溢出，结果会存放在这里。
- `a`, `b`: 参与运算的两个操作数。

这些函数返回一个 `bool` 值：

- 如果运算会导致溢出，函数返回 `true`。
- 如果运算可以安全执行，函数返回 `false`，并将计算结果存入 `*result`。

重要的是，`result`、`a` 和 `b` 的类型不必完全相同，函数会根据通用实数转换规则（usual arithmetic conversions）来确定操作的类型。

### 代码示例

让我们看一个使用这些新函数的例子。确保你使用的编译器支持 C23 标准（例如 GCC 13+ 或 Clang 16+），并在编译时启用 C23 模式（例如，使用 `-std=c23` 标志）。

```c
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <stdckdint.h>

int main(void) {
    int a = INT_MAX - 10;
    int b = 20;
    int result;

    // 检查加法
    if (ckd_add(&result, a, b)) {
        printf("ckd_add: %d + %d -> 溢出\n", a, b);
    } else {
        printf("ckd_add: %d + %d = %d\n", a, b, result);
    }

    // 检查减法
    int c = INT_MIN + 10;
    int d = 20;
    if (ckd_sub(&result, c, d)) {
        printf("ckd_sub: %d - %d -> 溢出\n", c, d);
    } else {
        printf("ckd_sub: %d - %d = %d\n", c, d, result);
    }

    // 检查乘法
    int e = INT_MAX / 10;
    int f = 20;
    if (ckd_mul(&result, e, f)) {
        printf("ckd_mul: %d * %d -> 溢出\n", e, f);
    } else {
        printf("ckd_mul: %d * %d = %d\n", e, f, result);
    }

    return 0;
}
```

**预期输出：**

```ansi
ckd_add: 2147483637 + 20 -> 溢出
ckd_sub: -2147483638 - 20 -> 溢出
ckd_mul: 214748364 * 20 -> 溢出
```

`ckd_*` 函数正确地检测到了导致溢出的值并返回 `true`，从而避免了未定义行为，并允许我们打印出有用的错误信息。

## 习题

<Exercise id="11761" :d="3" :w="2">

写一个程序：从标准输入读入两个 `int`，计算它们的和。

要求：

1. 使用 `ckd_add` 检查是否溢出；
2. 若溢出，输出一条错误信息到标准错误流并退出；
3. 否则输出结果。

</Exercise>

<Exercise id="11762" :d="4" :w="3">

写一个程序：从标准输入读入三个 `int` `a`、`b`、`c`，计算表达式 `a*b + c`。

要求：

1. 使用 `ckd_mul` 与 `ckd_add` 分两步计算并检查溢出；
2. 任一步溢出都视为失败；
3. 不要写出可能触发有符号溢出的中间表达式。

</Exercise>
