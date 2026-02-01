# `sizeof` 运算符

`sizeof` 是运算符，用于获取一个类型或表达式结果的大小（以字节为单位）。它的结果类型为 `size_t`，并且在标准层面是一个整数常量表达式（某些情况下除外，例如对 VLA 的 `sizeof`）。

## 1. 两种形式

1. `sizeof(type-name)`：对类型取大小。
2. `sizeof expression`：对表达式结果类型取大小。

### 1.1 建议用括号

为了避免与运算符优先级产生歧义，实践中建议统一写成 `sizeof(expr)` 以及 `sizeof(type)`。

## 2. 未求值上下文

对大多数表达式而言，`sizeof(expr)` 的 `expr` 不会被求值（因此其中的副作用不会发生）。

例外：如果 `expr` 的类型是可变修改类型 (Variably modified type, VLA 相关)，则 `expr` 可能会被求值以确定大小。

## 3. 常见坑

本章只讨论 `sizeof` 的基本用法。更复杂的细节会在后续章节中展开。

## 4. 习题

<Exercise id="10419" :d="4" :w="3">

写一个程序，分别输出：

1. `sizeof(int)`
2. `sizeof(double)`
3. `sizeof(char)`

然后解释这三个结果为何可能不同。

</Exercise>
