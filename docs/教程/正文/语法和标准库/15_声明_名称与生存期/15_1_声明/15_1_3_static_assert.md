# 15.1.3 static_assert（C11/C23）

静态断言 (Static assertion) 用于在编译期检查一个条件；若条件不成立，则产生编译错误。

它常用于：

1. 检查类型大小/对齐是否满足约束；
2. 检查宏配置是否互相冲突；
3. 把代码依赖的前提（例如某个常量的取值范围）写进源代码，由编译器检查。

## 1. 语法与版本

### 1.1 C11：`_Static_assert`

C11 引入 `_Static_assert`：

```c
_Static_assert(expression, message);
```

其中 `expression` 必须是整数常量表达式；`message` 必须是字符串字面量。

### 1.2 C23：`static_assert`

C23 引入 `static_assert`，与 `_Static_assert` 等效，并且允许省略 `message`：

```c
static_assert(expression, "message");
static_assert(expression);
```

同时，`_Static_assert` 在 C23 中成为保留的兼容拼写（已被弃用但仍可用）。

::: tip 头文件

在 C23 之前，`<assert.h>` 曾提供 `static_assert` 作为便捷宏；C23 起 `static_assert` 是关键字，不再由 `<assert.h>` 提供。

:::

## 2. 示例

以下示例按 C23 编写。

```c
#include <stddef.h>
#include <stdalign.h>

static_assert(sizeof(int) >= 4, "int must be at least 32-bit");
static_assert(alignof(max_align_t) >= alignof(double));

int main(void) {
    return 0;
}
```

## 3. 断言的放置位置

静态断言通常紧挨着被检查的声明放置。例如某个结构体的布局必须与外部二进制格式一致时，就把检查其 `sizeof` 的断言放在该类型定义之后；某个配置宏必须位于一定范围内时，就把断言放在该宏的定义附近。这样阅读声明时就能同时看到它依赖的前提。

```c
#include <stddef.h>

struct header {
    unsigned short tag;
    unsigned short len;
};

static_assert(sizeof(struct header) == 4, "header layout mismatch");
```

## 4. 与 `#error` 的分工

`#error` 在预处理阶段起作用，适合检查宏是否定义、配置组合是否合法；预处理器不认识类型，类型大小、对齐以及常量表达式之间的关系要用 `static_assert` 检查。

## 5. 习题

<Exercise id="11510" :d="2" :w="1">

为你项目里的一个结构体写 `static_assert`：约束 `sizeof` 与 `alignof`；并解释约束的依据是什么。

</Exercise>

<Exercise id="11511" :d="1" :w="2">

写一个“配置检查”示例：当两个宏同时被定义时触发 `static_assert(0, "...")`。

</Exercise>

<Exercise id="11512" :d="1" :w="2">

解释：为什么 `static_assert` 的条件必须是“整数常量表达式”？它为什么不能检查运行期条件？

</Exercise>
