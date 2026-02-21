# 浮点环境

浮点环境是与**浮点异常**和**浮点舍入方向**相关的操作的集合。

## `#pragma STDC FENV_ACCESS 实参`

`实参` 为 `ON` 和 `OFF` 和 `DEFAULT` 三者之一。注意大写。

当 `#pragma STDC FENV_ACCESS ON` 的时候，告诉编译器，程序会访问或修改浮点环境，因此某些由编译器进行的可能修改浮点环境的优化会被禁用。

## 浮点异常

有以下五种浮点异常：

### 1. 定义域错误 `FE_INVALID`

当某个在数学上未定义的运算发生时，产生这个异常。比如：

1. `0.0 / 0.0`
2. `log(-1.0)` （对非正数取对数）或 `sqrt(-1.0)` 或 `pow(-1.0, 0.5)`（给负数开方）等；
3. `asin(2)` 或 `acos(2)`（众所周知，反正弦函数和反余弦函数的定义域是 $[-1,1]$）或 `acosh(0.5)`（众所周知，反双曲余弦函数的定义域是 $[-1, + \infin )$）等；
4. `fmod(2, 0.0)`（取余运算的第二个操作数为 0）等；
5. `INFINITY / INFINITY` 或 `INFINITY * 0.0`
6. 任何涉及**信号 NaN** (Signaling NaN, sNaN) 的操作；
7. 某些涉及**静默 NaN** (Quiet Nan, qNan) 的操作（如 `NAN > 1.0`）等；

示例：

```c
#include <stdio.h>
#include <fenv.h>
#include <math.h>

#pragma STDC FENV_ACCESS ON

int main(void) {
    double result;

    feclearexcept(FE_ALL_EXCEPT);

    result = sqrt(-1.0);
    result = 0.0 / 0.0;
    result = log(-1.0);
    result = asin(2);
    result = pow(-1, 0.5);
    result = INFINITY / INFINITY;
    result = INFINITY * 0.0;


    if (fetestexcept(FE_INVALID)) {
        printf("FE_INVALID (Invalid operation) flag is set.\n");
    } else {
        printf("FE_INVALID flag is NOT set.\n");
    }

    printf("Result: %f\n", result);

    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


只保留一条对 `result` 的赋值操作并运行代码，如果打印出 `FE_INVALID (Invalid operation) flag is set.`，则产生了 `FE_INVALID` 异常。

### 2. 向上溢出 `FE_OVERFLOW`

当运算结果的绝对值大于目标浮点类型可表示的最大有限值时，会产生这个异常。常见表现是结果变为 `HUGE_VAL`、`INFINITY` 或其负值，并伴随异常标志变化。

### 3. 向下溢出 `FE_UNDERFLOW`

当非零结果在目标浮点类型中太小，无法以正规化形式表示时，会产生这个异常。结果可能变成次正规数，或在更极端时舍入为零。

### 4. 除以零 `FE_DIVBYZERO`

有限非零数除以零会触发该异常。典型结果是带符号无穷大，例如 `1.0 / 0.0` 得到 `+INFINITY`。

### 5. 精度损失 `FE_INEXACT`

当结果不能被目标浮点格式精确表示，必须舍入时，会触发该异常。这是最常见的浮点异常之一。
## 浮点异常的相关操作

`<fenv.h>` 提供了标准化的异常标志操作接口，常用函数如下：

```c
int feclearexcept(int excepts);
int fetestexcept(int excepts);
int feraiseexcept(int excepts);
int fegetexceptflag(fexcept_t *p, int excepts);
int fesetexceptflag(const fexcept_t *p, int excepts);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


推荐流程通常是：先清标志、再执行计算、最后检查标志。这样能避免历史异常状态干扰当前判断。

```c
#include <fenv.h>
#include <math.h>
#include <stdio.h>

#pragma STDC FENV_ACCESS ON

int main(void) {
    feclearexcept(FE_ALL_EXCEPT);

    volatile double x = 1.0e308;
    volatile double y = x * x;
    (void)y;

    if (fetestexcept(FE_OVERFLOW)) {
        puts("FE_OVERFLOW detected");
    }
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


## 浮点舍入方向

浮点数在计算机中是用有限的位数来表示的，不能精确地表示所有实数。当一个真实的数值或计算结果落在两个可表示的浮点数之间时，就必须选择其中一个来近似它，这个过程称为**舍入**。

关于可表示的浮点数，参见 [IEEE 754](/教程/番外/1_IEEE754.md)。

有以下四种浮点舍入方向：

### 1. 向零舍入 `FE_TOWARDZERO`

结果向 0 的方向截断。对正数相当于向下取整，对负数相当于向上取整（靠近 0）。

### 2. 向正无穷舍入 `FE_UPWARD`

结果向 `+∞` 方向舍入。它对“需要保证下界不被低估”的计算很有意义。

### 3. 向负无穷舍入 `FE_DOWNWARD`

结果向 `-∞` 方向舍入。它常用于“需要保证上界不被高估”的区间计算。

### 4. 向最近舍入 `FE_TONEAREST`

结果舍入到最接近的可表示值；遇到中点时按实现规则处理（常见策略是就近到偶数）。这通常是默认模式。

### 浮点舍入方向的相关操作

舍入模式查询与设置函数如下：

```c
int fegetround(void);
int fesetround(int mode);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


`fesetround` 成功后会影响后续浮点运算。建议把“切换模式”的作用域限制在尽可能小的代码区间，并在结束时恢复原模式。

```c
int old = fegetround();
if (old != -1) {
    if (fesetround(FE_DOWNWARD) == 0) {
        /* ... 浮点计算 ... */
        (void)fesetround(old);
    }
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


## 保存和恢复浮点环境

当函数需要临时修改异常状态或舍入模式时，应显式保存与恢复环境。`fenv_t` 表示一个完整浮点环境快照，常用接口包括：

```c
int fegetenv(fenv_t *envp);
int fesetenv(const fenv_t *envp);
int feholdexcept(fenv_t *envp);
int feupdateenv(const fenv_t *envp);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


这组接口适合写可组合的数值函数：被调代码不会把调用方环境状态悄悄改坏。
