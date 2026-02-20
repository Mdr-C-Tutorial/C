# 有条件编译

有条件编译用于在预处理阶段按条件选择代码片段，常见场景是平台差异、特性开关和头文件包含保护。

## 1. `defined` 运算符

`defined 标识符` 或 `defined(标识符)` 会在预处理表达式中产生 `1` 或 `0`，用于判断宏是否已定义。

```c
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
#define USE_C23 1
#else
#define USE_C23 0
#endif
```

## 2. 条件指令族

`#if`、`#elif`、`#else`、`#endif` 构成通用分支；`#ifdef` 与 `#ifndef` 是“只检查是否定义”的简写；C23 增加了 `#elifdef` 与 `#elifndef`，可让多分支条件更紧凑。若目标仍是 C11/C17，可写成 `#elif defined(...)` 以保持兼容。

```c
#ifdef _WIN32
#define PATH_SEP "\\"
#elifdef __unix__
#define PATH_SEP "/"
#else
#define PATH_SEP "/"
#endif
```

## 3. 包含保护

头文件通常使用 `#ifndef` 保护，防止同一翻译单元重复包含：

```c
#ifndef MY_LIB_H
#define MY_LIB_H

/* 声明 */

#endif
```

包含保护不是可选装饰，而是头文件最基础的正确性要求。
