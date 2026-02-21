# `#pragma`

这个预处理指令可以控制一些**实现定义的行为**。

## 标准行为

在下面这三种用法中，_`arg`_ 是 `ON` 和 `OFF` 和 `DEFAULT` 三者之一。

### `#pragma STDC FENV_ACCESS`_`arg`_

如果这个宏被设置为 `ON`，会告知编译器：这个程序会访问或者修改[**浮点环境**](/教程/正文/语法和标准库/17_数学库/17_7_fenv.md)。这可能导致某些和浮点数相关的编译器优化被禁用。

默认值由实现定义，一般为 `OFF`。

### `#pragma STDC FP_CONTRACT`_`arg`_

这个指令控制是否允许把多个浮点运算收缩 (contract) 成单条复合指令，例如把 `a * b + c` 收缩为融合乘加。开启后可能得到更高性能，也可能改变最后一位舍入结果。

```c
#pragma STDC FP_CONTRACT ON
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


如果你的数值算法要求严格可复现结果，应结合编译器文档确认默认值并显式设置。

### `#pragma STDC CX_LIMITED_RANGE`_`arg`_

这个指令用于复数运算的范围语义。设置为 `ON` 时，实现可在更宽松的中间范围假设下优化复数表达式；设置为 `OFF` 时，优先保持更严格的数值语义。它主要影响 `<complex.h>` 相关计算。

这两个 pragma 都是“给编译器的语义提示”，不是强制“必须生成某条特定机器指令”。

## 非标准行为

以下两个 `#pragma` 的用法不是标准里面规定的，但是受到了普遍支持。

### `#pragma once`

这个指令受到绝大多数现代编译器的支持，可以看作事实标准。

如果它出现在头文件中，表示这个头文件无论在同一个源文件中被包含多少次，都只应该被解析一次。它的目标与传统的 `#ifndef` / `#define` 包含保护一致，但属于实现扩展。

### `#pragma pack`

这个指令控制后续定义的结构体和联合体成员的最大对齐。

典型写法如下：

```c
#pragma pack(push, 1)
struct hdr {
    unsigned short type;
    unsigned int len;
};
#pragma pack(pop)
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


对齐调整会影响布局、访问性能，甚至在某些平台导致未对齐访问异常。除非必须与外部二进制格式对齐，否则不要随意使用。

## `_Pragma()`

`_Pragma` 是一个运算符，可在宏展开中生成 pragma，等价于“可编程的 `#pragma`”。它最常见的用途是把编译器诊断控制封装成宏。

```c
#define DO_PRAGMA(x) _Pragma(#x)
#define DISABLE_UNUSED_WARNING DO_PRAGMA(GCC diagnostic ignored "-Wunused-parameter")
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


当需要在宏内部携带 pragma 时，应使用 `_Pragma`，而不是直接写 `#pragma`。
