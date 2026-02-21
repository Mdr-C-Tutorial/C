# static_assert（C11/C23）

静态断言 (Static assertion) 用于在编译期检查一个条件；若条件不成立，则产生编译错误。

它常用于：

1. 检查类型大小/对齐是否满足约束；
2. 检查宏配置是否互相冲突；
3. 作为“代码级别的约束文档”，把恒定约束写死在编译期。

## 1. 语法与版本

### 1.1 C11：`_Static_assert`

C11 引入 `_Static_assert`：

```c
_Static_assert(expression, message);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


其中 `expression` 必须是整数常量表达式；`message` 必须是字符串字面量。

### 1.2 C23：`static_assert`

C23 引入 `static_assert`，与 `_Static_assert` 等效，并且允许省略 `message`：

```c
static_assert(expression, "message");
static_assert(expression);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


同时，`_Static_assert` 在 C23 中成为保留的兼容拼写（已被弃用但仍可用）。

::: tip 头文件

在 C23 之前，`<assert.h>` 曾提供 `static_assert` 作为便捷宏；C23 起 `static_assert` 是关键字，不再由 `<assert.h>` 提供。

:::

## 2. 示例

```c
#include <stddef.h>
#include <stdalign.h>

static_assert(sizeof(int) >= 4, "int must be at least 32-bit");
static_assert(alignof(max_align_t) >= alignof(double));

int main(void) {
    return 0;
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


## 3. 约束应贴近声明点

静态断言最有价值的用法是“贴着被约束对象放置”。例如某个结构体要和外部二进制协议对齐，就把 `static_assert(sizeof(...))` 放在该类型定义附近；某个配置宏必须满足范围，就把断言放在配置头中。这样读者在阅读声明时就能同时看到约束，不需要跨文件反向推理。

```c
#include <stddef.h>

struct header {
    unsigned short tag;
    unsigned short len;
};

static_assert(sizeof(struct header) == 4, "header layout mismatch");
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


## 4. 与 `#error` 的分工

`#error` 属于预处理阶段，适合检查“宏是否定义、配置组合是否合法”；`static_assert` 属于语言语义阶段，适合检查“类型大小、对齐、常量表达式关系是否满足约束”。把这两层分开使用，诊断信息会更聚焦，也更容易维护。

## 5. 习题

1. 为你项目里的一个结构体写 `static_assert`：约束 `sizeof` 与 `alignof`；并解释约束的依据是什么。
2. 写一个“配置检查”示例：当两个宏同时被定义时触发 `static_assert(0, "...")`。
3. 解释：为什么 `static_assert` 的条件必须是“整数常量表达式”？它为什么不能检查运行期条件？
