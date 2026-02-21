# 数学函数

`<math.h>` 提供了大量面向实数（主要是浮点类型）的数学函数与工具宏。它的核心使用方式是：用 `double`/`float`/`long double` 表示实数，再用标准库函数完成常见运算（指数、对数、三角、开方、取整等）。

::: warning 注意

`<math.h>` 中很多函数对输入值有定义域要求。遇到“看起来算不出来”的情况时，应先检查：

1. 输入是否超出定义域（如 `sqrt(x)` 的 `x` 为负）；
2. 结果是否溢出为无穷大（如 `exp(1000)`）；
3. 是否产生 NaN（如 `0.0 / 0.0`）。

:::

## 1. 常用宏与分类函数

### 1.1 特殊值：`INFINITY` / `NAN`

`INFINITY` 与 `NAN` 是两个宏，分别表示正无穷与 NaN（Not a Number）。它们通常用于：表达“不可表示的结果”、或者在计算中作为哨兵值。

### 1.2 浮点分类：`isfinite` / `isinf` / `isnan` / `fpclassify`

对浮点计算结果进行分类，是写健壮数学代码的常用技巧：

```c
#include <math.h>

int isfinite(real-floating x);
int isinf(real-floating x);
int isnan(real-floating x);
int fpclassify(real-floating x);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


（标准里的形参类型写作 `real-floating`，表示“实浮点类型”；实际函数对 `float`/`double`/`long double` 都可用。）

`fpclassify` 的结果会落在若干宏之一：`FP_NAN`、`FP_INFINITE`、`FP_ZERO`、`FP_SUBNORMAL`、`FP_NORMAL`。

## 2. 常见函数类别（选读清单）

本节只列常见类别，不试图覆盖所有接口：

1. 三角函数：`sin` / `cos` / `tan` 及其反函数 `asin` / `acos` / `atan`（注意：大多数函数输入/输出都以“弧度”为单位）
2. 指数与对数：`exp` / `log` / `log10`
3. 幂与开方：`pow` / `sqrt` / `cbrt`
4. 取整与舍入：`ceil` / `floor` / `trunc` / `round`
5. 绝对值与余数：`fabs` / `fmod` / `remainder`
6. 距离：`hypot`（用于计算 $\sqrt{x^2+y^2}$，比手写 `sqrt(x*x+y*y)` 更稳健）

## 3. 错误处理：返回值优先，其次看错误状态

多数 `<math.h>` 函数会通过“返回值”表达计算结果；当发生定义域错误/上溢等情况时，标准允许实现以多种方式报告错误（例如设置 `errno`，或者产生浮点异常）。因此在项目代码中更推荐：

1. 先检查返回值是否为 `NaN` / `INFINITY`；
2. 必要时再结合 `errno` 或 `<fenv.h>`（见 17.7）。

## 4. 示例：角度转弧度并计算正弦

```c
#include <math.h>
#include <stdio.h>

int main(void) {
    double deg;
    if (scanf("%lf", &deg) != 1) {
        return 1;
    }

    const double pi = acos(-1.0);
    double rad = deg * pi / 180.0;

    printf("rad = %.6f\n", rad);
    printf("sin = %.6f\n", sin(rad));
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


## 习题

<Exercise id="11701" :d="2" :w="2">

使用 `<math.h>` 中的函数解决 [**10401**](/教程/题解/语法和标准库/表达式/10401.md)。

</Exercise>

<Exercise id="11702" :d="3" :w="2">

写一个程序：从标准输入读入两个 `double`，输出它们的欧氏距离 $\sqrt{x^2+y^2}$。

要求：

1. 使用 `hypot`（不要直接写 `sqrt(x*x + y*y)`）；
2. 输入失败时退出；
3. 输出保留 6 位小数。

</Exercise>

<Exercise id="11703" :d="4" :w="3">

写一个程序：从标准输入读入一个 `double`，输出 `sqrt(x)`；若 `x` 为负，输出一条错误信息到标准错误流并退出。

要求：

1. 使用 `isnan` 或 `fpclassify` 对结果进行检查；
2. 不要假设输入一定成功；
3. 出错信息写到 `stderr`（提示：`fprintf(stderr, ...)`）。

</Exercise>
