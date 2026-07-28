# 2.7 true / false（布尔常量）

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

输出：

::: terminal

1 0

:::
## 2. 条件表达式中的“真/假”

在 `if`、`while` 等语境中，条件表达式会被当作“与 0 比较”的结果：

1. 结果为 0：假；
2. 结果非 0：真。

因此，下列写法在语义上等价：

```c
if (x) { /* ... */ }
if (x != 0) { /* ... */ }
```
## 3. `_Bool` 的归一化语义

把算术表达式赋给 `_Bool`（或 `bool`）时，结果会被归一化为 `0` 或 `1`。这也是为什么布尔对象做输出时经常只会看到 `0` / `1`。

```c
#include <stdbool.h>
#include <stdio.h>

int main(void) {
    bool a = 0;
    bool b = 7;
    bool c = -3;
    printf("%d %d %d\n", a, b, c); /* 0 1 1 */
    return 0;
}
```

输出：

::: terminal

0 1 1

:::
## 4. 布尔语义与位语义要分开

`bool` 表达的是逻辑真值，不适合直接承载位掩码语义。若代码需要组合多个标志位，应使用无符号整数对象并通过位运算处理；若只表示“成立/不成立”，使用 `bool` 更清晰。把这两类语义分离后，接口含义会稳定很多。


## 5. 习题

<Exercise id="10203" :d="3" :w="2">

写出能判断 `int x` 是否为偶数的布尔表达式：当 `x` 为偶数时结果为 `true`，否则为 `false`。要求对 `0`、`1`、`2`、`-3` 等若干输入分别给出该表达式的求值结果。

</Exercise>
