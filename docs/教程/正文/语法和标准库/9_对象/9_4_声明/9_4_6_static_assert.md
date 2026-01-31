# static_assert（C11/C23）

静态断言 (Static assertion) 用于在编译期检查一个条件；若条件不成立，则产生编译错误。

它常用于：

1. 检查类型大小/对齐是否满足约束；
2. 检查宏配置是否互相冲突；
3. 作为“代码级别的约束文档”，把不变量写死在编译期。

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

```c
#include <stddef.h>
#include <stdalign.h>

static_assert(sizeof(int) >= 4, "int must be at least 32-bit");
static_assert(alignof(max_align_t) >= alignof(double));

int main(void) {
    return 0;
}
```

## 3. 习题

1. 为你项目里的一个结构体写 `static_assert`：约束 `sizeof` 与 `alignof`；并解释约束的依据是什么。
2. 写一个“配置检查”示例：当两个宏同时被定义时触发 `static_assert(0, "...")`。
3. 解释：为什么 `static_assert` 的条件必须是“整数常量表达式”？它为什么不能检查运行期条件？
