# 5.2 do while

`do while` 是“先执行、后判断”的循环语句。无论条件是否成立，循环体都会先执行一次，因此它适合“至少执行一次”的流程。

## 1. 基本语法

```c
do {
    statement;
} while (condition);
```

注意结尾分号属于语法的一部分，不能省略。

## 2. 示例

```c
#include <stdio.h>

int main(void) {
    int n = 3;

    do {
        printf("n = %d\n", n);
        n--;
    } while (n > 0);

    return 0;
}
```

输出：

::: terminal

n = 3
n = 2
n = 1

:::

即使把初值改成 `n = 0`，循环体仍会先执行一次，然后才根据条件决定是否继续。

## 3. 适用场合

当流程需要先执行一次操作，再根据结果决定是否重复时，例如菜单交互、失败重试和输入校验，宜使用 `do while`。如果循环体可能一次也不需要执行，优先考虑 `while`，让读者一眼看出“可能不进循环”。

## 4. 与 `break` / `continue` 的配合

在 `do while` 中，`break` 会直接跳出循环；`continue` 会立刻跳到条件判断处，再决定是否进入下一轮。理解这点可以避免把 `continue` 误解为“回到循环体开头”。

## 5. 一个典型输入循环

```c
#include <stdio.h>

int main(void) {
    int x;
    do {
        if (scanf("%d", &x) != 1) {
            break;
        }
        printf("read: %d\n", x);
    } while (x != 0);
    return 0;
}
```

例如依次输入 `7` 和 `0`，交互结果为：

::: terminal

7
read: 7
0
read: 0

:::

这个模式表达的是“至少尝试读取一次，读到 0 结束；读取失败也结束”，很适合简单交互流程。

## 6. 与 `while` 的改写关系

`do while` 往往可以改写为“先执行一次循环体，再进入 `while`”的形式。

```c
/* do while 版本 */
do {
    step();
} while (cond());

/* 等价思路 */
step();
while (cond()) {
    step();
}
```

当循环体较长或包含多条退出路径时，直接保留 `do while` 通常更易读。

## 7. `do { ... } while (0)`

宏定义中常见 `do { ... } while (0)` 的写法。它的条件恒为 `0`，循环体只执行一次，作用是把多条语句包装成一条语句，使宏调用后加上分号，在 `if`/`else` 中也能像普通语句一样使用。宏的定义方法见 [17.2 define](/教程/正文/语法和标准库/17_预处理与翻译/17_2_define)。
