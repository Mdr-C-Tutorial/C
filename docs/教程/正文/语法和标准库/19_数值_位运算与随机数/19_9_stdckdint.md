# 19.9 带检查的整数算术

当算术运算的结果超出其类型所能表示的范围时，就会发生**整数溢出**。在 C 语言中，有符号整数的溢出是未定义行为（Undefined Behavior, UB），结果可能是一个回绕（wrap-around）的值，也可能是程序崩溃。许多安全漏洞（如整数溢出漏洞）都源于此。

C23 新增了头文件 `<stdckdint.h>`，提供一组执行带检查整数算术的函数。它们会告诉你一次运算是否溢出，运算本身不会触发未定义行为。

::: warning 注意

`<stdckdint.h>` 属于 C23 新增内容。如果你的实现没有该头文件，可以把这一节当作“了解 C23 新能力”；在工程中则需要采用实现扩展或手写检查逻辑。

:::

## 1. C23 之前的解决方案

在 C23 之前，程序通常依赖编译器提供的内建函数，或者手写检查代码来检测整数溢出。例如，GCC 和 Clang 提供了 `__builtin_add_overflow` 等内建函数。

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

在 `int` 为常见 32 位的 GCC 或 Clang 实现中，输出为：

::: terminal

发生溢出！

:::

`__builtin_add_overflow` 是编译器扩展，不属于标准 C，使用它会影响代码的可移植性。

## 2. C23 `<stdckdint.h>`

`<stdckdint.h>` 定义了三个函数，分别用于检查加法、减法和乘法运算是否会溢出：

- `ckd_add(result, a, b)`：检查 `a + b` 是否溢出。
- `ckd_sub(result, a, b)`：检查 `a - b` 是否溢出。
- `ckd_mul(result, a, b)`：检查 `a * b` 是否溢出。

### 2.1 函数原型

这些函数都遵循相似的模式。以 `ckd_add` 为例：

```c
bool ckd_add(type *result, type1 a, type2 b);
```

- `result`：一个指向结果存储位置的指针。如果运算没有溢出，结果会存放在这里。
- `a`、`b`：参与运算的两个操作数。

这些函数返回一个 `bool` 值：

- 如果运算会导致溢出，函数返回 `true`。
- 如果运算可以安全执行，函数返回 `false`，并将计算结果存入 `*result`。

`result`、`a` 和 `b` 的类型不必相同。运算按 `a` 和 `b` 的数学值进行，不经过通常算术转换；只要数学结果不能用 `*result` 的类型表示，就算溢出。

### 2.2 代码示例

下面的程序需要编译器支持 C23，并在编译时启用 C23 模式（例如使用 `-std=c23`）。

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
        puts("ckd_add: 溢出");
    } else {
        printf("ckd_add: %d + %d = %d\n", a, b, result);
    }

    // 检查减法
    int c = INT_MIN + 10;
    int d = 20;
    if (ckd_sub(&result, c, d)) {
        puts("ckd_sub: 溢出");
    } else {
        printf("ckd_sub: %d - %d = %d\n", c, d, result);
    }

    // 检查乘法
    int e = INT_MAX / 10;
    int f = 20;
    if (ckd_mul(&result, e, f)) {
        puts("ckd_mul: 溢出");
    } else {
        printf("ckd_mul: %d * %d = %d\n", e, f, result);
    }

    return 0;
}
```

三个运算的结果都超出 `int` 的表示范围，因此输出为：

::: terminal

ckd_add: 溢出
ckd_sub: 溢出
ckd_mul: 溢出

:::

## 3. 习题

<Exercise id="11911" :d="2" :w="2">

写一个程序：从标准输入读入两个 `int`，计算它们的和。

要求：

1. 使用 `ckd_add` 检查是否溢出；
2. 若溢出，输出一条错误信息到标准错误流并退出；
3. 否则输出结果。

</Exercise>

<Exercise id="11912" :d="3" :w="2">

写一个程序：从标准输入读入三个 `int` `a`、`b`、`c`，计算表达式 `a*b + c`。

要求：

1. 使用 `ckd_mul` 与 `ckd_add` 分两步计算并检查溢出；
2. 任一步溢出都视为失败；
3. 不要写出可能触发有符号溢出的中间表达式。

</Exercise>
