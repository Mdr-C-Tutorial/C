# true / false（布尔常量）

C 语言的布尔类型是 `_Bool`（C99 引入）。在大多数代码中，推荐通过头文件 `<stdbool.h>` 使用更易读的别名：

1. `bool`：`_Bool` 的宏别名（C99 起）；
2. `true`、`false`：布尔常量宏（C99 起）。

在 C23 中，`bool` / `true` / `false` 也被引入为关键字（并保留 `_Bool` 作为兼容拼写）。

## 1. 示例

```c
#include <stdbool.h>
#include <stdio.h>

int main(void) {
    bool ok = true;
    bool bad = false;

    printf("%d %d\n", ok, bad);
    return 0;
}
```

## 2. 条件表达式中的“真/假”

在 `if`、`while` 等语境中，条件表达式会被当作“与 0 比较”的结果：

1. 结果为 0：假；
2. 结果非 0：真。

因此，下列写法在语义上等价：

```c
if (x) { /* ... */ }
if (x != 0) { /* ... */ }
```

## 3. 习题

<Exercise id="10423" :d="3" :w="2">

写一个函数 `bool is_even(int x)`：当 `x` 为偶数返回 `true`，否则返回 `false`。要求在 `main` 中测试若干组输入并输出结果。

</Exercise>
