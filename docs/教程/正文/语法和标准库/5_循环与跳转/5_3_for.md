# 5.3 for

`for` 把“初始化、条件判断、步进表达式”集中写在一行，是 C 中最常见的计数循环结构。它与 `while` 可以互相改写，但在边界控制上通常更紧凑。

## 1. 基本语法

```c
for (init; condition; step) {
    statement;
}
```

执行顺序是：先执行 `init`，再判断 `condition`；若条件非零就执行循环体，然后执行 `step`，再回到条件判断。

## 2. 示例

```c
#include <stdio.h>

int main(void) {
    int sum = 0;

    for (int i = 1; i <= 100; ++i) {
        sum += i;
    }

    printf("%d\n", sum);
    return 0;
}
```

输出：

::: terminal

5050

:::

`i` 在 `for` 的初始化部分声明，它的作用域和生存期都仅限于这条 `for` 语句，循环结束后不能再使用 `i`。

## 3. 常见变体

`for (;;)` 表示无限循环；三个表达式都可以省略，但分号必须保留。省略任何一项都应有明确意图，否则会降低可读性。

## 4. 逆序循环的边界

用无符号整数类型（如 `size_t`）的索引逆序遍历时，不能写成 `for (size_t i = n - 1; i >= 0; --i)`：`i >= 0` 恒为真，`i` 为 `0` 时再减 `1` 会回绕到该类型的最大值，循环不会结束，并会越界访问数组。下面这种写法更稳妥：

```c
#include <stddef.h>

void visit_reverse(const int *a, size_t n) {
    for (size_t i = n; i > 0; --i) {
        int v = a[i - 1];
        (void)v;
    }
}
```

它通过 `i > 0` 保证 `i - 1` 总是有效索引，避免把无符号对象减到极大值后继续循环。

## 5. 多控制表达式写法

`for` 还可以在初始化和步进位置维护多个控制对象。这种写法在双指针扫描、窗口移动等场景中很常见，但要保证每个控制对象的职责清晰。

```c
#include <stdio.h>

int main(void) {
    for (int left = 0, right = 4; left <= right; ++left, --right) {
        printf("(%d, %d)\n", left, right);
    }
    return 0;
}
```

输出：

::: terminal

(0, 4)
(1, 3)
(2, 2)

:::

当控制对象过多时，建议提炼出辅助函数，避免把 `for` 头部堆成难读表达式。

## 6. `continue` 与步进表达式

`for` 中执行 `continue` 时，不会直接回到条件判断，而是先执行 `step`，再进入下一轮条件检查。因此，每轮都必须执行的推进动作应放在 `step` 中；若只写在循环体尾部，执行 `continue` 时会被绕过。
