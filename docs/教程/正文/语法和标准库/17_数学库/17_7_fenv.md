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

只保留一条对 `result` 的赋值操作并运行代码，如果打印出 `FE_INVALID (Invalid operation) flag is set.`，则产生了 `FE_INVALID` 异常。

### 2. 向上溢出 `FE_OVERFLOW`

### 3. 向下溢出 `FE_UNDERFLOW`

### 4. 除以零 `FE_DIVBYZERO`

### 5. 精度损失 `FE_INEXACT`

## 浮点异常的相关操作

## 浮点舍入方向

浮点数在计算机中是用有限的位数来表示的，不能精确地表示所有实数。当一个真实的数值或计算结果落在两个可表示的浮点数之间时，就必须选择其中一个来近似它，这个过程称为**舍入**。

关于可表示的浮点数，参见 [IEEE 754](/教程/番外/1_IEEE754.md)。

有以下四种浮点舍入方向：

### 1. 向零舍入 `FE_TOWARDZERO`

### 2. 向正无穷舍入 `FE_UPWARD`

### 3. 向负无穷舍入 `FE_DOWNWARD`

### 4. 向最近舍入 `FE_TONEAREST`

### 浮点舍入方向的相关操作

## 保存和恢复浮点环境
