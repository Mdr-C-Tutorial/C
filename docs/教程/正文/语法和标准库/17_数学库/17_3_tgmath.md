# 泛型数学

`<tgmath.h>`（type-generic math）提供一组“泛型”宏，让你可以用一个名字调用到正确的数学函数版本：

1. 当实参是 `float` 时，选择带 `f` 后缀的版本（如 `sinf`）。
2. 当实参是 `double` 时，选择不带后缀的版本（如 `sin`）。
3. 当实参是 `long double` 时，选择带 `l` 后缀的版本（如 `sinl`）。
4. 当实参是复数类型时（见 17.2），选择 `<complex.h>` 中的对应版本（如 `csin`）。

它的价值在于：当你写“泛型代码”或希望尽量少写后缀时，代码更紧凑；但它的本质是宏展开，仍然需要你理解底层函数的语义与定义域要求。

## 示例

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

::: warning 注意

1. `<tgmath.h>` 的接口是宏；宏会参与表达式求值，因此要避免把带副作用的表达式当作实参反复展开（例如 `sin(i++)` 这类写法）。
2. 如果你不需要泛型，直接使用 `<math.h>` 的明确函数名通常更直观。

:::

## 习题

<Exercise id="11731" :d="3" :w="2">

写一个程序：分别以 `float` 与 `double` 读入同一个数值，调用 `<tgmath.h>` 的 `sqrt` 并输出结果。

要求：

1. 程序应同时包含 `float` 与 `double` 两条路径；
2. 输出时分别标注类型；
3. 输入失败时退出。

</Exercise>
