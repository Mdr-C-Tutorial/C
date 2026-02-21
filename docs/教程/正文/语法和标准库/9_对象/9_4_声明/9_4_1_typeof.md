# typeof（C23）

`typeof`（以及 `typeof_unqual`）是 C23 引入的 `typeof` 说明符 (typeof specifier)。它们用于“从一个类型名或表达式推导出一个类型名”，并可直接用于声明。

## 1. 基本形式

C23 提供两组形式：

1. `typeof(type-name)` 与 `typeof(expression)`
2. `typeof_unqual(type-name)` 与 `typeof_unqual(expression)`

它们统称为 `typeof` 运算符 (typeof operators)。

## 2. 规则要点

### 2.1 `typeof(expression)` 不做隐式转换

对 `typeof(expression)` 而言，标准规定：

1. 不对 `expression` 进行隐式转换（因此不会发生“数组到指针”“函数到指针”的默认转换）。
2. 若操作数的类型是可变修改类型 (Variably modified type, VLA 相关)，则该操作数会被求值；否则它是未求值上下文 (Unevaluated context)。

这意味着：

```c
int a[3];
typeof(a) b = {0}; /* b 的类型是 int[3]，不是 int*。 */
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


### 2.2 `typeof_unqual` 会移除限定符

`typeof_unqual` 会在 `typeof` 的基础上移除类型限定符：例如移除 `const`、`volatile`，并产生“非原子的无限定类型”（也会移除 `_Atomic` 的限定效果）。

示例：

```c
const int x = 1;
typeof(x) a = x;         /* a 的类型是 const int */
typeof_unqual(x) b = x;  /* b 的类型是 int */
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


### 2.3 不能用于位域

`typeof` 运算符不能应用于位域 (Bit-field) 成员。

## 3. 常见用途

1. 避免重复写类型名（尤其是复杂指针/数组/函数指针类型）。
2. 在宏里做“类型保持”的局部临时变量（注意：仍需避免对宏参数的重复求值）。

## 4. 习题

1. 写一个宏 `MDR_SWAP(a, b)`，要求：
   1. 只对 `a`、`b` 求值一次；
   2. 仅接受可赋值的左值；
   3. 使用 `typeof` 推导临时变量类型。
2. 写出并解释：`typeof("abc")` 的结果类型是什么？为什么它不是 `char*`？
3. 写出并解释：`typeof_unqual((const int)1)` 的结果类型是什么？
