# 19.6 泛型数学

`<tgmath.h>`（type-generic math）提供一组“泛型”宏，让你可以用一个名字调用到正确的数学函数版本：

1. 当实参是 `float` 时，选择带 `f` 后缀的版本（如 `sinf`）。
2. 当实参是 `double` 时，选择不带后缀的版本（如 `sin`）。
3. 当实参是 `long double` 时，选择带 `l` 后缀的版本（如 `sinl`）。
4. 当实参是复数类型时（见 19.5），选择 `<complex.h>` 中的对应版本（如 `csin`）。

这些宏只决定调用哪个函数，被调函数的定义域、精度和错误报告方式与直接调用该函数时相同。

## 1. 示例

```c
#include <tgmath.h>
#include <stdio.h>

int main(void) {
    float x = 0.5f;
    double y = 0.5;

    printf("%.6f\n", sin(x)); /* 实参是 float：等价于 sinf(x) */
    printf("%.6f\n", sin(y)); /* 实参是 double：等价于 sin(y) */
    return 0;
}
```

常见数学库会输出：

::: terminal

0.479426
0.479426

:::

::: warning 注意

1. `<tgmath.h>` 的接口是宏，不应把带副作用的表达式（例如 `i++`）作为实参。
2. 不需要泛型时，直接使用 `<math.h>` 中带明确后缀的函数名更清楚，尤其是需要强制使用某一精度（例如 `long double`）的场合。

:::

## 2. 实参类型决定选择结果

选择依据是实参类型，而不是接收结果的对象的类型。即使把结果赋给 `double` 对象，只要实参是 `float`，调用的仍是 `float` 版本。需要固定使用某一精度时，应先把实参显式转换为目标类型：

```c
#include <tgmath.h>

double f(float x) {
    return sin((double)x); /* 调用 double 版本的 sin */
}
```

## 3. 习题

<Exercise id="11905" :d="2" :w="2">

写一个程序：分别以 `float` 与 `double` 读入同一个数值，调用 `<tgmath.h>` 的 `sqrt` 并输出结果。

要求：

1. 程序应同时包含 `float` 与 `double` 两条路径；
2. 输出时分别标注类型；
3. 输入失败时退出。

</Exercise>
