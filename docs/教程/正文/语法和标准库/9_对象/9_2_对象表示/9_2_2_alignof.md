# alignof

`alignof` 用于查询一个类型的对齐要求 (Alignment requirement)。它返回一个 `size_t` 类型的整数常量表达式 (Integer constant expression)。

## 1. 语法与版本

### 1.1 C11：`_Alignof`

C11 引入了 `_Alignof(type-name)` 运算符。

### 1.2 C11：`<stdalign.h>` 中的 `alignof` 宏

在 C23 之前，标准库头文件 `<stdalign.h>` 提供了 `alignof` 宏，作为 `_Alignof` 的便捷拼写。

### 1.3 C23：`alignof` 关键字

C23 将 `alignof` 引入为关键字，对应 C11 的 `_Alignof`。

::: tip 建议

如果你的目标标准是 C11 及以上，并且希望写出“看起来更像 C23”的代码，可以在 C11/C17 中包含 `<stdalign.h>` 使用 `alignof` 宏；在 C23 中直接使用 `alignof` 关键字。

:::

## 2. 规则要点

1. `alignof` 的操作数是**类型名** (Type-name)，不是表达式。
2. 若 `type-name` 是数组类型，`alignof(type-name)` 等于其元素类型的对齐要求。
3. `type-name` 不能是函数类型或不完全类型 (Incomplete type)。
4. `alignof` 是未求值上下文 (Unevaluated context)：它不会求值其操作数。

## 3. 示例

```c
#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>

struct s {
    char c;
    int n;
};

int main(void) {
    printf("alignof(char) = %zu\n", alignof(char));
    printf("alignof(int) = %zu\n", alignof(int));
    printf("alignof(struct s) = %zu\n", alignof(struct s));
    printf("alignof(float[10]) = %zu\n", alignof(float[10]));
    printf("alignof(max_align_t) = %zu\n", alignof(max_align_t));
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


## 4. 习题

1. 对比 `alignof(struct { char c; int n; })` 与 `alignof(struct { int n; char c; })`，它们一定相同吗？请用实验验证并解释原因。
2. 为什么 `alignof` 只能接受类型名，而不是表达式？写出你的理解。
3. 写出一个宏 `MDR_ALIGNOF(expr)`：把表达式的类型转换成 `alignof` 能接收的形式（提示：C23 有 `typeof`）。
