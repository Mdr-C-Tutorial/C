# alignas（C11/C23）

`alignas`（C11 中为 `_Alignas`，并可通过 `<stdalign.h>` 的宏 `alignas` 使用）是对齐说明符 (Alignment specifier)。它用于**提高**某个被声明对象的对齐要求。

## 1. 语法与版本

1. C11：`_Alignas(expression)`、`_Alignas(type-name)`
2. C23：`alignas(expression)`、`alignas(type-name)`

在 C23 之前，`alignas` 通常来自 `<stdalign.h>` 的宏；C23 起 `alignas` 成为关键字。

## 2. 规则要点

1. `alignas(type-name)` 等价于“把对齐要求设为 `alignof(type-name)`”。
2. `alignas(expression)` 中，`expression` 必须是整数常量表达式，其值要么是一个有效对齐值，要么是 0。
3. 若 `expression` 为 0，则该说明符没有效果。
4. `alignas` **不能削弱**类型的天然对齐：如果你指定了更小的对齐值，实际对齐仍至少是该类型本来的对齐要求。
5. 同一声明中出现多个 `alignas` 时，取其中最严格的对齐要求。

## 3. 示例

```c
#include <stdalign.h>
#include <stdio.h>

struct sse_t {
    alignas(16) float v[4];
};

int main(void) {
    printf("alignof(struct sse_t) = %zu\n", alignof(struct sse_t));

    alignas(64) struct sse_t x;
    (void)x;

    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


## 4. 与内存分配接口的配合

`alignas` 只约束被声明对象的对齐，不会自动改变任意动态分配返回指针的对齐策略。若对象通过动态分配获得存储，应配合满足对齐要求的分配方案。换句话说，声明期对齐与运行期分配是两条需要同时满足的约束链路，不能只看其中一端。

## 5. 习题

1. 解释：为什么标准禁止 `alignas`“降低对齐要求”？降低会带来什么风险？
2. 写一个结构体，让它的某个成员 `char cacheline[64]` 强制以 64 字节对齐；并解释“这是让数组对象对齐”，还是“让数组元素对齐”。
3. 用实验验证：当同一声明出现多个 `alignas` 时，到底取哪一个（例如 `alignas(16) alignas(64) int x;`）。
