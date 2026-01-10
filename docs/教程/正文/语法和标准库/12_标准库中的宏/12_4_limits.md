# C 语言标准库头文件：limits.h

`<limits.h>` 是 C 语言标准库中的经典头文件，从最早的 C 标准起就存在，在 C23（ISO/IEC 9899:2024）中核心内容未变。它定义了各种基本整数类型和字符类型的数值极限宏，这些宏的值由实现决定，用于确保代码的可移植性（避免硬编码假设如 `INT_MAX = 2147483647`）。

使用方式：

```c
#include <limits.h>
```

## 1. 字符类型极限

- `CHAR_BIT`：一个字节中的位数（通常 8，至少 8）。
- `SCHAR_MIN`、`SCHAR_MAX`：`signed char` 的最小值和最大值。
- `SCHAR_MIN` 通常为 -128，`SCHAR_MAX` 为 127。
- `UCHAR_MAX`：`unsigned char` 的最大值（通常 255）。
- `CHAR_MIN`、`CHAR_MAX`：`char` 的最小值和最大值（取决于`char`是否有符号；若有符号，则同`SCHAR_`；若无符号，则`CHAR_MIN` 为 0，`CHAR_MAX` 同`UCHAR_MAX`）。
- `MB_LEN_MAX`：多字节字符序列的最大字节数（至少 1，通常 4 或 6，支持 UTF-8 等）。

## 2. 整数类型极限

这些宏对应标准整数类型的最小和最大值：

- 短整型：
  - `SHRT_MIN`、`SHRT_MAX`：`signed short`（至少 -32767 ~ 32767）。
  - `USHRT_MAX`：`unsigned short`（至少 65535）。

- 整型：
  - `INT_MIN`、`INT_MAX`：`signed int`（至少 -32767 ~ 32767，通常 -2147483647 ~ 2147483647）。
  - `UINT_MAX`：`unsigned int`（至少 65535，通常 4294967295）。

- 长整型：
  - `LONG_MIN`、`LONG_MAX`：`signed long`（至少 -2147483647 ~ 2147483647）。
  - `ULONG_MAX`：`unsigned long`（至少 4294967295）。

- 长长整型（C99 及以后）：
  - `LLONG_MIN`、`LLONG_MAX`：`signed long long`（至少 -9223372036854775807 ~ 9223372036854775807）。
  - `ULLONG_MAX`：`unsigned long long`（至少 18446744073709551615）。

## 使用示例

```c
#include <limits.h>
#include <stdio.h>

int main() {
    printf("int 的最小值：%d\n", INT_MIN);
    printf("int 的最大值：%d\n", INT_MAX);
    printf("unsigned int 的最大值：%u\n", UINT_MAX);
    printf("一个字节的位数：%d\n", CHAR_BIT);
    return 0;
}
```

可能的输出：

<TerminalWindow title="zsh - test_limits">

int 的最小值：-2147483648

int 的最大值：2147483647

unsigned int 的最大值：4294967295

一个字节的位数：8

</TerminalWindow>
