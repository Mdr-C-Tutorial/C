# 6.7.13.7 noreturn 与 \_Noreturn 属性

[[toc]]

## 描述

1. 当 `_Noreturn` 被当作属性记号使用，而不是函数说明符使用时，它与 `noreturn` 的约束和语义完全相同。

2. 但这种把 `_Noreturn` 当属性写的形式，已经属于将来可能移除的旧特性。

## 约束

1. `noreturn` 只能用于函数。

2. 它不能带属性参数子句。

## 语义

1. 只要某个函数的任一声明带了 `noreturn`，那么它的第一条声明就也必须带 `noreturn`。

2. 若一个翻译单元把该函数声明成 `noreturn`，另一个翻译单元却没这么声明，则行为未定义。

3. 若调用点之前看见的是 `noreturn` 声明，而该函数最终却返回了，行为未定义。

4. 若实现支持该属性，则：

```c
__has_c_attribute(noreturn)
```

应返回 `202311L`。

## 推荐实践

1. 若一个被声明为 `noreturn` 的函数看起来仍可能返回给调用者，实现应给出诊断。

2. 因此：
   - `[[noreturn]] void f(void) { abort(); }` 合理；
   - `[[noreturn]] void g(int i) { if (i > 0) abort(); }` 应被诊断，因为 `i <= 0` 时会返回；
   - `[[noreturn]] int h(void);` 也值得诊断，因为它的返回类型本身就暗示了“可能返回”。
