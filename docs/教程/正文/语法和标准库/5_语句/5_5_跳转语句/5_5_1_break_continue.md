# break 和 continue

`break` 与 `continue` 都用于循环控制，但语义不同：`break` 终止当前循环，`continue` 结束当前迭代并进入下一轮条件判断。

## 1. `break`

`break` 可用于 `switch` 与循环语句。在循环中，它会立刻退出最内层循环。

```c
for (int i = 0; i < 10; ++i) {
    if (i == 5) {
        break;
    }
    printf("%d ", i);
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


输出到 `4` 后结束循环。

## 2. `continue`

`continue` 不会结束循环，只会跳过当前迭代剩余语句。

```c
for (int i = 0; i < 10; ++i) {
    if ((i % 2) != 0) {
        continue;
    }
    printf("%d ", i);
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


这段代码只打印偶数。

## 3. 嵌套循环注意点

两者都只作用于“最内层”循环。若需要一次退出多层循环，通常应重构为函数并配合 `return`，或在确认可读性收益后使用受控 `goto` 清理路径。

另外，`break` 在 `switch` 中也有效，但它只退出 `switch`，不会退出外层循环。`switch` 嵌套在循环里时，这一点尤其容易误判。

## 4. `for` 中的 `continue`

`continue` 在 `for` 里并不是“直接回到条件判断”，而是先执行步进表达式，再进入下一轮判断。这一点与 `while` / `do while` 的直觉不同，写循环控制时应明确考虑步进副作用。

```c
#include <stdio.h>

int main(void) {
    for (int i = 0; i < 5; ++i) {
        if ((i % 2) != 0) {
            continue;
        }
        printf("%d ", i);
    }
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


## 5. 清理语义不要埋在 `continue` 后面

使用 `continue` 时，要特别留意本轮迭代后半段是否包含必要收尾动作。若清理动作写在 `continue` 之后，就会被跳过，导致状态不一致。

更稳妥的写法是：把“本轮都必须执行”的更新放在 `continue` 前，或提炼成单独函数确保每轮收尾路径一致。

## 6. `switch` 嵌套在循环中的语义

当 `switch` 位于循环体内部时，`break` 只会离开 `switch`，不会离开循环；而 `continue` 会直接进入循环下一轮。这组差异在状态机代码里很常见，写分支时应明确“我要结束的是哪一层结构”。

```c
for (;;) {
    switch (state) {
        case 0:
            state = 1;
            break;      /* 仅离开 switch，循环继续 */
        case 1:
            state = 2;
            continue;   /* 进入 for 的下一轮 */
        default:
            break;
    }
    if (state == 2) {
        break;          /* 这里才会离开 for */
    }
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


## 7. 提前返回与循环跳转的取舍

若函数主体几乎都在一个循环里，且检测到错误后无需继续本函数后续逻辑，直接 `return` 往往比多层 `break` 更直观。相反，如果退出循环后仍有统一收尾逻辑，保留 `break` 再集中处理会更清楚。关键是让“退出后还要不要做事”在代码结构中一眼可见。
