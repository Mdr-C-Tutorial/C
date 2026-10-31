# 19.8 位操作

## 1. 位操作函数

`<stdbit.h>` 是 C23 引入的头文件，提供一组面向**无符号整数类型**的位操作工具。

::: warning 注意

1. 这些接口要求实现支持 C23；老版本编译器可能不存在该头文件。
2. 多数接口要求实参为无符号整数类型；把有符号整数直接传入可能会触发不符合预期的转换。

:::

常用能力包括：

1. 统计 1/0 的个数：`stdc_count_ones` / `stdc_count_zeros`
2. 统计前导/尾随 0/1：`stdc_leading_zeros` / `stdc_trailing_zeros` / `stdc_leading_ones` / `stdc_trailing_ones`
3. 位宽与取整：`stdc_bit_width` / `stdc_bit_floor` / `stdc_bit_ceil`
4. 单比特判断：`stdc_has_single_bit`

它们还提供按类型区分的后缀版本：

- `_uc`：形参类型为 `unsigned char`
- `_us`：形参类型为 `unsigned short`
- `_ui`：形参类型为 `unsigned int`
- `_ul`：形参类型为 `unsigned long`
- `_ull`：形参类型为 `unsigned long long`

不带类型后缀的同名标识符（如 `stdc_count_ones(x)`）是类型泛型宏，除了接受上述 5 种标准无符号整数类型外，还接受扩展无符号整数类型以及 C23 无符号位精确整数 `unsigned _BitInt(N)`。

### 1.1 14 组位操作接口与边界约定

设实参 `value` 的位宽为 $W$，`<stdbit.h>` 提供的 14 组操作及其对 `value == 0`（或全 1）的边界返回值如下：

| 泛型宏名 | 功能说明 | `value == 0` 时的返回值 |
| --- | --- | --- |
| `stdc_leading_zeros(v)` | 从最高有效位起连续 0 的个数 | $W$ |
| `stdc_leading_ones(v)` | 从最高有效位起连续 1 的个数 | `0` |
| `stdc_trailing_zeros(v)` | 从最低有效位起连续 0 的个数 | $W$ |
| `stdc_trailing_ones(v)` | 从最低有效位起连续 1 的个数 | `0` |
| `stdc_first_leading_zero(v)` | 从最高位起第一个 0 的位置（从 1 开始编号；无则返回 0） | `1`（全 1 时返回 `0`） |
| `stdc_first_leading_one(v)` | 从最高位起第一个 1 的位置（从 1 开始编号；无则返回 0） | `0` |
| `stdc_first_trailing_zero(v)` | 从最低位起第一个 0 的位置（从 1 开始编号；无则返回 0） | `1`（全 1 时返回 `0`） |
| `stdc_first_trailing_one(v)` | 从最低位起第一个 1 的位置（从 1 开始编号；无则返回 0） | `0` |
| `stdc_count_zeros(v)` | 所有二进制位中 0 的总个数 | $W$ |
| `stdc_count_ones(v)` | 所有二进制位中 1 的总个数（popcount） | `0` |
| `stdc_has_single_bit(v)` | 是否恰好只有一个二进制位为 1（即是否为 2 的幂，返回 `bool`） | `false` |
| `stdc_bit_width(v)` | 表示该值所需的最少位数（即 $1 + \lfloor\log_2 v\rfloor$） | `0` |
| `stdc_bit_floor(v)` | 不大于 `v` 的最大 2 的整数次幂 | `0` |
| `stdc_bit_ceil(v)` | 不小于 `v` 的最小 2 的整数次幂（若结果超出类型可表示范围则行为未定义） | `1` |

与编译器内建函数 `__builtin_clz(0)` / `__builtin_ctz(0)` 在输入为 `0` 时产生未定义行为不同，`<stdbit.h>` 中的所有前导/尾随计数与查找函数对 `0` 都有良定义返回值。

## 2. 示例：统计二进制 1 的个数

```c
#include <stdio.h>
#include <stdbit.h>

int main(void) {
    unsigned int x = 0xF0u; /* 11110000b */
    printf("ones = %u\n", stdc_count_ones_ui(x));
    return 0;
}
```

输出：

::: terminal

ones = 4

:::

## 3. 端序

端序是构成多字节对象的字节的顺序。更多相关内容请参考 [Wikipedia](https://zh.wikipedia.org/wiki/%E5%AD%97%E8%8A%82%E5%BA%8F)。

在 `<stdbit.h>` 中定义了三个宏，用来表示标量类型的端序：

- `__STDC_ENDIAN_BIG__`
- `__STDC_ENDIAN_LITTLE__`
- `__STDC_ENDIAN_NATIVE__`

1. 如果标量类型为**小端序**，则 `__STDC_ENDIAN_NATIVE__` 等于 `__STDC_ENDIAN_LITTLE__`；
2. 如果标量类型为**大端序**，则 `__STDC_ENDIAN_NATIVE__` 等于 `__STDC_ENDIAN_BIG__`。
3. 如果既不使用大端序，也不使用小端序，则 `__STDC_ENDIAN_NATIVE__` 既不等于 `__STDC_ENDIAN_BIG__`，也不等于 `__STDC_ENDIAN_LITTLE__`。

示例：

```c
#include <stdio.h>
#include <stdbit.h>

int main(void) {
    if (__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_BIG__) {
        printf("大端序\n");
    } else if (__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__) {
        printf("小端序\n");
    } else {
        printf("未知的端序\n");
    }
    return 0;
}
```

在常见的 x86、x86-64 和 AArch64 小端序环境中，输出为：

::: terminal

小端序

:::

## 4. 习题

<Exercise id="11909" :d="2" :w="1">

写一个程序：从标准输入读入一个 `unsigned int`，输出它的二进制 1 的个数。

要求：

1. 使用 `<stdbit.h>` 的 `stdc_count_ones_ui`；
2. 输入失败时退出。

</Exercise>

<Exercise id="11910" :d="3" :w="2">

写一个程序：从标准输入读入一个 `unsigned int` `x` 和一个非负整数 `k`，输出把 `x` 循环左移 `k` 位的结果。

要求：C23 的 `<stdbit.h>` 没有循环移位函数，请用移位和按位或运算实现。

</Exercise>
