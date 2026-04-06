# 6.7.3.6 typeof 说明符

> [!TIP]
> `typeof` 和 `typeof_unqual` 不是“求值后拿值”，而是“从表达式或类型名中提取类型”。只有在操作数本身是变长修改类型时，才会真的求值。

[[toc]]

## 语法

1. `typeof` 说明符有两种形式：

```c
typeof(typeof-specifier-argument)
typeof_unqual(typeof-specifier-argument)
```

2. 它们的参数可以是：
   - 一个表达式；
   - 一个类型名。

3. 标准把 `typeof` 与 `typeof_unqual` 合称为 **typeof 运算符**。

## 约束

1. `typeof` 运算符不能作用到“指代位字段成员”的表达式上。

## 语义

1. 若作用对象是表达式，则 `typeof(E)` 产生 `E` 的类型。

2. 若作用对象是类型名，则它产生该类型名所表示的同一类型；若内部嵌套了 `typeof`，则按内到外逐层处理。

3. 如果操作数类型是变长修改类型，那么操作数会被求值；否则不求值。

4. `typeof_unqual(T)` 产生的是：
   - 对 `typeof(T)` 所得类型，去掉所有限定符；
   - 并且去掉原子性；
   - 得到的非原子、无限定版本。

5. `typeof` 会保留全部限定符。

## 理解上的几个关键点

1. 对函数形参中声明为数组类型或函数类型的对象，`typeof` 得到的是调整后的类型，也就是指针类型。

2. 嵌套 `typeof` 时，内部操作数若需要求值，会先完成求值。

3. `typeof` 不会自动进行数组到指针衰减，除非标准的相关调整规则本来就要求那么做。

## 例子

1. `typeof(1 + 1)` 等价于 `int`。

2. 若：

```c
const _Atomic int purr = 0;
const int meow = 1;
```

那么：

```c
typeof_unqual(purr)
```

得到的是 `int`，而：

```c
typeof(meow)
```

仍保留 `const`。

3. `typeof("meow")` 的类型是 `char[5]`，不是 `char *`。

4. 对变长数组使用 `typeof` 时，与长度有关的部分在执行期才真正落实。
