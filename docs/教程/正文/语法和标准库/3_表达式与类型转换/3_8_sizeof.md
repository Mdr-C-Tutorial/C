# 3.8 `sizeof` 运算符

`sizeof` 运算符给出类型或表达式结果类型的大小，单位是字节 (Byte)，结果的类型是定义在 `<stddef.h>` 中的无符号整数类型 `size_t`。它常用于计算对象大小、数组长度和动态分配所需的字节数。

## 1. 两种写法与一个习惯

`sizeof(type-name)` 对类型取大小，`sizeof expr` 对表达式结果类型取大小。第二种写法语法上可省括号，但为了避免优先级歧义，通常写成 `sizeof(expr)` 更清楚。

```c
size_t a = sizeof(int);
size_t b = sizeof(1 + 2.0);   /* 等价于 sizeof(double) */
```

使用 `printf` 打印 `sizeof` 的结果时，应配合专门对应 `size_t` 的格式说明符 `%zu`：

```c
#include <stdio.h>

int main(void) {
    int count = 42;
    printf("sizeof(char) = %zu, sizeof(count) = %zu\n", sizeof(char), sizeof(count));
    return 0;
}
```

输出（`int` 为 4 字节的实现）：

::: terminal

sizeof(char) = 1, sizeof(count) = 4

:::

## 2. 字节的定义与 `sizeof(char)`

在 C 标准中，1 个字节定义为足以容纳基本执行字符集任意成员的最小可寻址存储单元。标准严格规定：

$$\texttt{sizeof(char)} == \texttt{sizeof(signed char)} == \texttt{sizeof(unsigned char)} == 1$$

其他类型的 `sizeof` 结果表示该类型对象占用的字节数（即等于 `char` 大小的多少倍）。需要注意，C 标准中的 1 字节并不强制等于 8 比特，具体比特数由 `<limits.h>` 中的宏 `CHAR_BIT` 给出（在绝大多数现代通用平台上 `CHAR_BIT` 为 8）。

## 3. 操作数不求值原则

::: concept sizeof-unevaluated title="sizeof 的未求值上下文"

除操作数类型为变长数组（见第 7 章）外，`sizeof` 的操作数处于**未求值上下文 (Unevaluated Context)**：编译器仅在翻译期根据操作数的类型推导出字节大小，不会在运行期真正执行该表达式，表达式中的任何副作用都不会发生。

:::

```c
#include <stdio.h>

int main(void) {
    int x = 10;
    size_t sz = sizeof(x++); /* x++ 不会被执行，x 的值保持 10 */

    printf("sz = %zu, x = %d\n", sz, x);
    return 0;
}
```

输出：

::: terminal

sz = 4, x = 10

:::

因此，绝不应在 `sizeof` 的操作数中编写包含自增、赋值或函数调用副作用的表达式。

## 4. 适用范围与约束

`sizeof` 只能作用于完整的对象类型（或具有该类型的表达式），不能用于以下三种情形：

1. **函数类型**：例如不能对函数名 `main` 求 `sizeof(main)`。
2. **不完整类型 (Incomplete Type)**：例如 `sizeof(void)` 或尚未给出长度的数组声明 `extern int arr[];`，因为编译器此时无法得知对象占用的存储大小。
3. **位域成员 (Bit-field)**：结构体中的位域成员可能不按整字节对齐，不能对其应用 `sizeof`（见 10.2.1 节）。

## 5. 习题

<Exercise id="10313" :d="4" :w="3">

写一个程序，分别输出：

1. `sizeof(int)`
2. `sizeof(double)`
3. `sizeof(char)`

然后解释这三个结果为何可能不同。

</Exercise>
