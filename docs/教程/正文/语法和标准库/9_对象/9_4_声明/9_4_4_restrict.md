# restrict（C99）

`restrict` 是 C99 引入的类型限定符 (Type qualifier)。它只用于**指针类型**，用于向编译器作出一个“别名保证”：在一段作用域内，通过该 `restrict` 指针（以及从它派生出的指针）访问的对象，不会再被其他互不相关的指针访问。

这使得编译器可以更激进地优化（例如向量化），但代价是：一旦你的保证被破坏，程序就会产生未定义行为 (Undefined Behavior, UB)。

## 1. 基本写法

`restrict` 修饰指针本身，因此位置通常是：

```c
int* restrict p = 0;
const int* restrict q = 0;
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


注意：`int restrict *p;` 是把 `restrict` 修饰在 `int` 上，这不是 `restrict` 的用法（在 C23 中更明确属于错误写法）。

## 2. 语义要点（可操作版）

在一个函数形参的典型用法中：

```c
void f(size_t n, int* restrict dst, const int* restrict src);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


你可以把它理解为：在 `f` 执行期间，`dst[0..n)` 与 `src[0..n)` 所指向的对象区域不重叠；否则就是 UB。

::: warning 编译器可以忽略 `restrict`

标准允许编译器忽略 `restrict` 的优化机会；但不允许程序违反 `restrict` 的别名保证。也就是说：你不能写“违反 restrict，但期待编译器不优化所以没事”的代码。

:::

## 3. 示例：正确与错误

```c
#include <stddef.h>

void copy_n(size_t n, int* restrict dst, const int* restrict src) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

void ok(void) {
    int a[4] = {1, 2, 3, 4};
    int b[4] = {0};
    copy_n(4, b, a); /* OK：不重叠 */
}

void bad(void) {
    int a[4] = {1, 2, 3, 4};
    copy_n(3, a + 1, a); /* UB：区域重叠，违反 restrict 的别名保证 */
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


## 4. 常见误区

1. **把 `restrict` 当作“更快的关键字”**：它不是提示，而是语义承诺。
2. **对宏参数/表达式乱加 `restrict`**：`restrict` 的语义作用于左值表达式；对“非左值”的 `restrict` 限定不会产生你想要的效果。
3. **把 `restrict` 用在函数指针上**：标准不允许把 `restrict` 用于函数类型相关的指针语义。

## 5. 习题

1. 写一个 `memcpy` 风格的函数 `m_copy`，并用 `restrict` 标注形参；再写一个 `memmove` 风格的 `m_move`，说明为什么它通常不适合使用 `restrict`。
2. 举例说明：在什么情况下 `restrict` 能让编译器做出明显更强的优化？（提示：循环向量化）
3. 找一个你写过的函数，尝试加上 `restrict`；然后证明（通过代码审查或测试用例）你确实满足了 restrict 的不重叠前提。
