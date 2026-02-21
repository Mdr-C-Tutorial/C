# do while

`do while` 是“先执行、后判断”的循环语句。无论条件是否成立，循环体都会先执行一次，因此它适合“至少执行一次”的流程。

## 1. 基本语法

```c
do {
    statement;
} while (condition);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


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

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


即使把初值改成 `n = 0`，循环体仍会先执行一次，然后才根据条件决定是否继续。

## 3. 使用建议

当循环体中包含“先执行一次动作，再决定是否继续”这类流程时，`do while` 往往比 `while` 更贴近业务语义，例如菜单交互、重试逻辑和输入校验。若业务允许零次执行，优先考虑 `while`，让读者一眼看出“可能不进循环”。

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

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


这个模式表达的是“至少尝试读取一次，读到 0 结束；读取失败也结束”，很适合简单交互流程。

## 6. 与 `while` 的改写关系

`do while` 往往可以改写为“先执行一次循环体，再进入 `while`”的形式。理解这种对应关系有助于你在代码评审中快速确认控制流是否等价。

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

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


当循环体较长或包含多条退出路径时，直接保留 `do while` 通常更易读。

## 7. `do { ... } while (0)` 的语义位置

你还会在宏定义中看到 `do { ... } while (0)`，它并不是“循环逻辑”，而是借 `do while` 语法把多条语句包装成一个语句单元。这样宏在 `if/else` 等语境下能像普通语句一样安全使用。虽然外观相同，但这里的 `while (0)` 只是结构封装手段，不承担重复执行语义。
