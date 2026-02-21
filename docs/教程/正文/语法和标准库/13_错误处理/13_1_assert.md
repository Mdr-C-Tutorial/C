# `<assert.h>`

`<assert.h>` 提供了 `assert` 宏，用于在调试阶段检查程序的**不变量** (Invariant) 和**前置条件** (Precondition)。它的定位是“尽快失败并暴露 bug”，而不是“把错误优雅地处理掉”。

## 1. 基本用法

```c
#include <assert.h>

int div_int(int a, int b) {
    assert(b != 0);
    return a / b;
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


当断言表达式为假（0）时，程序会打印诊断信息并终止。

## 2. 断言失败时会发生什么

当 `assert(expr)` 失败时，它会：

1. 向标准错误输出（`stderr`）输出一条诊断信息（其具体格式由实现决定）。
2. 终止程序（通常通过调用 `abort`）。

因此，断言失败通常意味着：程序已经进入了“不应该到达”的状态，继续运行只会产生更多未知后果。

::: tip 如何理解断言的角色

把 `assert` 视为“写给作者自己的合同”。只要合同被撕毁，就应该立刻停机检查，而不是硬着头皮继续跑下去。

:::

## 3. `NDEBUG`：断言可能被移除

如果在包含 `<assert.h>` 之前定义了宏 `NDEBUG`，那么 `assert` 会被禁用，通常等价于：

```c
assert(any_expression);
/* 展开后近似为 */
((void)0);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


这带来两个直接后果：

1. **断言表达式可能不会被求值**。
2. **不要在断言表达式里编写依赖副作用的代码**。

示例：

```c
#include <assert.h>
#include <stdio.h>

int main(void) {
    int i = 0;

    assert(i++ == 0); // 若断言被禁用，i++ 不会执行。

    printf("%d\n", i);
    return 0;
}
```

在启用断言的情况下，可能输出：

<TerminalWindow>

1

</TerminalWindow>

在禁用断言的情况下，可能输出：

<TerminalWindow>

0

</TerminalWindow>

## 4. 常见使用场景

1. **检查内部前置条件**：例如函数要求“参数指针一定非空”。
2. **维护不变量**：例如数据结构的大小关系、索引边界等。
3. **标记不可达分支**：例如 `switch` 的 `default` 分支在逻辑上不可能发生。

示例（不可达分支）：

```c
#include <assert.h>

enum color {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
};

const char* color_name(enum color c) {
    switch (c) {
        case COLOR_RED: {
            return "red";
        }
        case COLOR_GREEN: {
            return "green";
        }
        case COLOR_BLUE: {
            return "blue";
        }
        default: {
            assert(!"unreachable");
            return "unknown";
        }
    }
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


::: warning 断言不是输入校验

“用户输入不合法”“文件打不开”这类失败属于外部世界的不确定性，应该用 `if` 分支、返回值与错误码进行处理，而不是用 `assert` 把程序直接杀死。

:::

## 5. 习题

1. 阅读下面代码，分别说明在“启用断言”和“禁用断言（定义 `NDEBUG`）”时程序的输出与行为差异：

```c
#include <assert.h>
#include <stdio.h>

int main(void) {
    int x = 0;
    assert(++x == 1);
    printf("%d\n", x);
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


2. 在你自己的项目中实现一个宏 `MDR_ASSERT(cond)`：
   1. 默认行为与 `assert` 类似。
   2. 当定义 `NDEBUG` 时被禁用。
   3. 要求：不得对 `cond` 进行多次求值。
3. 写一个函数 `int pop(stack* s, int* out);`：
   1. 哪些条件适合用 `assert`，哪些条件必须通过返回值向调用者报告错误？
   2. 给出你选择的错误处理策略，并写出函数签名与返回值含义。
