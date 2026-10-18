# 19.2 C语言标准库头文件：limits.h

`<limits.h>` 自 C89 起就是标准头文件，定义字符类型和标准整数类型的取值范围。这些宏的值由实现决定，程序应使用这些宏，而不是假定 `INT_MAX` 等于 `2147483647`。

```c
#include <limits.h>
```

## 1. 字符类型极限

- `CHAR_BIT`：一个字节中的位数（通常 8，至少 8）。
- `SCHAR_MIN`、`SCHAR_MAX`：`signed char` 的最小值和最大值，通常为 -128 和 127。
- `UCHAR_MAX`：`unsigned char` 的最大值（通常 255）。
- `CHAR_MIN`、`CHAR_MAX`：`char` 的最小值和最大值。若 `char` 按有符号处理，二者分别等于 `SCHAR_MIN` 与 `SCHAR_MAX`；否则 `CHAR_MIN` 为 0，`CHAR_MAX` 等于 `UCHAR_MAX`。
- `MB_LEN_MAX`：一个多字节字符的最大字节数（至少 1）。

## 2. 整数类型极限

这些宏对应标准整数类型的最小和最大值：

- 短整型：
  - `SHRT_MIN`、`SHRT_MAX`：`signed short`（至少 -32767 ~ 32767）。
  - `USHRT_MAX`：`unsigned short`（至少 65535）。

- 整型：
  - `INT_MIN`、`INT_MAX`：`signed int`（至少 -32767 ~ 32767，通常 -2147483648 ~ 2147483647）。
  - `UINT_MAX`：`unsigned int`（至少 65535，通常 4294967295）。

- 长整型：
  - `LONG_MIN`、`LONG_MAX`：`signed long`（至少 -2147483647 ~ 2147483647）。
  - `ULONG_MAX`：`unsigned long`（至少 4294967295）。

- 长长整型（C99 及以后）：
  - `LLONG_MIN`、`LLONG_MAX`：`signed long long`（至少 -9223372036854775807 ~ 9223372036854775807）。
  - `ULLONG_MAX`：`unsigned long long`（至少 18446744073709551615）。

## 3. 使用示例

```c
#include <limits.h>
#include <stdio.h>

int main(void) {
    printf("int 的最小值: %d\n", INT_MIN);
    printf("int 的最大值: %d\n", INT_MAX);
    printf("unsigned int 的最大值: %u\n", UINT_MAX);
    printf("一个字节的位数: %d\n", CHAR_BIT);
    return 0;
}
```

可能的输出：

::: terminal title="zsh - test_limits"

int 的最小值: -2147483648

int 的最大值: 2147483647

unsigned int 的最大值: 4294967295

一个字节的位数: 8

:::
