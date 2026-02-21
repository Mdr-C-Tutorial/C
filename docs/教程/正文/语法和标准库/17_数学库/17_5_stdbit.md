# 位操作

## 位操作函数

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
5. 循环移位：`stdc_rotate_left` / `stdc_rotate_right`

它们通常还提供若干“按类型区分”的版本，例如：

- `stdc_count_ones_ui`：以 `unsigned int` 为主要目标类型
- `stdc_count_ones_ul`：以 `unsigned long` 为主要目标类型
- `stdc_count_ones_ull`：以 `unsigned long long` 为主要目标类型

## 示例：统计二进制 1 的个数

```c
#include <stdio.h>
#include <stdbit.h>

int main(void) {
    unsigned int x = 0xF0u; /* 11110000b */
    printf("ones = %u\n", stdc_count_ones_ui(x));
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


## 端序

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

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


## 习题

<Exercise id="11751" :d="3" :w="2">

写一个程序：从标准输入读入一个 `unsigned int`，输出它的二进制 1 的个数。

要求：

1. 使用 `<stdbit.h>` 的 `stdc_count_ones_ui`；
2. 输入失败时退出。

</Exercise>

<Exercise id="11752" :d="4" :w="2">

写一个程序：从标准输入读入一个 `unsigned int` `x` 和一个非负整数 `k`，输出把 `x` 循环左移 `k` 位的结果。

要求：使用 `stdc_rotate_left_ui`。

</Exercise>
