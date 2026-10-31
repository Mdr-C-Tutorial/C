# 5.5.1 break 和 continue

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

输出：

::: terminal

0 1 2 3 4

:::

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

输出：

::: terminal

0 2 4 6 8

:::

这段代码只打印偶数。

## 3. 嵌套循环注意点

两者都只作用于“最内层”循环。若需要一次退出多层循环，通常应重构为函数并配合 `return`，或在确认确实更易读时使用 `goto`，见 [5.5.2 goto](/教程/正文/语法和标准库/5_循环与跳转/5_5_跳转语句/5_5_2_goto)。

## 4. `for` 中的 `continue`

在 `for` 里，`continue` 先执行步进表达式，再进入下一轮条件判断。`while` / `do while` 的 `continue` 直接跳到条件判断，两者的差别容易让人在写循环控制时漏掉步进副作用。

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

输出：

::: terminal

0 2 4

:::

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

## 7. 习题

### 7.1 巩固

<Exercise id="10524" :d="2" :w="1">

先写出下面程序的输出，再编译运行核对。

```c
#include <stdio.h>

int main(void) {
    for (int i = 0; i < 8; ++i) {
        if (i == 6) {
            break;
        }
        if (i == 3) {
            continue;
        }
        printf("%d ", i);
    }
    printf("\n");
    return 0;
}
```

(1) 原程序的输出是什么？

(2) 把 `++i` 改成 `i += 2`，输出是什么？

(3) 把第一个 `break` 改成 `continue`，输出是什么？

(4) 把第一个 `break` 改成 `return 0`，屏幕上打印的数字与 (1) 相同，差别在哪里？注意程序结尾那条 `printf("\n")` 有没有执行。

</Exercise>

<Exercise id="10525" :d="2" :w="2">

下面的循环想把 0 到 9 中的偶数打印出来，运行时却停不下来。

```c
int i = 0;
while (i < 10) {
    if ((i % 2) != 0) {
        continue;
    }
    printf("%d ", i);
    ++i;
}
```

(1) 程序卡住之前，循环体里的 `printf` 执行了几次、打印的是什么？说明 `i` 变成 1 之后每一轮发生了什么。

(2) 给出三种改法，都能打印 `0 2 4 6 8`：在 `continue` 之前自增、改用 `for`、把条件反过来只包住 `printf`。

(3) 如果把 `++i` 移到循环体的第一行（`while` 之后、`if` 之前），输出会变成什么？对照 (2) 的三种改法，说明自增放在 `continue` 之后为什么会卡死，放在条件判断之前又为什么会改变结果。

</Exercise>

### 7.2 编程

<Exercise id="10526" :d="2" :w="2">

在 `1 <= i < j <= 40` 的范围里找出第一对满足 `i * i + j * j == n` 的整数，找到后立即结束两层循环。

```c
int find_pair(int n); /* 找到时打印 "i j" 并返回 1，找不到打印 "none" 并返回 0 */
```

(1) 用函数加 `return` 实现，在 `main` 里测试 `n = 1105` 与 `n = 3`，并各自打印一行 `checks = <内层循环体执行的次数>`。两个测试应分别输出 `4 33` 与 `none`，次数为 143 与 780。

(2) 再写一个不用 `return`、只用双重 `for` 和 `found` 标志的版本，输出格式不变，两个测试的次数应与 (1) 相同。

(3) 把 (1) 里的 `return 1` 改成 `break`，重新运行 `n = 1105`，程序会打印出几对数？说明原因，并据此回答：为什么一次退出两层循环通常要把搜索写进函数。

(4) 在 (2) 的版本里给内层循环加一句判断：当 `i * i + j * j > n` 时 `break`。`n = 1105` 时检查次数降到多少？`n = 3` 时呢？说明这个 `break` 为什么不会漏掉答案。

</Exercise>

<Exercise id="10527" :d="3" :w="2">

下面的程序模拟一台库存机：事件 1 投币 5 元，2 出货，3 打印状态，0 结束。用输入 `1 2 2 3 9 2 0 1` 运行。

```c
#include <stdio.h>

int main(void) {
    int event;
    int stock = 2;
    int balance = 0;
    int sold = 0;

    while (scanf("%d", &event) == 1) {
        switch (event) {
        case 0:
            break;
        case 1:
            balance += 5;
            break;
        case 2:
            if (stock == 0 || balance < 5) {
                printf("rejected\n");
                break;
            }
            --stock;
            balance -= 5;
            ++sold;
            break;
        case 3:
            printf("stock=%d balance=%d sold=%d\n", stock, balance, sold);
            break;
        default:
            printf("unknown event: %d\n", event);
            break;
        }
        printf("event %d done\n", event);
    }
    printf("bye stock=%d balance=%d sold=%d\n", stock, balance, sold);
    return 0;
}
```

(1) 贴出完整输出。读到 0 之后程序为什么没有结束？输入末尾那个 1 起了什么作用？

(2) 输出里哪些 `event ... done` 行在正确实现里不该出现？逐行说明它由哪一个 `break` 造成。

(3) 修复程序：让 0 结束整个循环，让被拒绝的事件与未知事件跳过本轮余下的打印。写出同一输入下修复后的完整输出。

(4) 修复后，只把 `case 2` 拒绝分支的 `continue` 改回 `break`，输出会多出哪些行？据此说明 `switch` 里的 `break` 与循环里的 `break` 分别结束哪一层结构。

</Exercise>

### 7.3 综合与挑战

<Exercise id="10528" :d="4" :w="2">

下面的成绩统计程序要输出每条输入的轮次、有效成绩的个数、总和与最高分，轮次编号应覆盖除结束标记以外的每一条输入。用 `95 120 60 -5 88 -1` 运行，结果与预期不符。

```c
#include <stdio.h>

int main(void) {
    int x;
    int round = 0;
    int count = 0;
    int sum = 0;
    int max = 0;

    while (scanf("%d", &x) == 1) {
        if (x == -1) {
            break;
        }
        if (x > max) {
            max = x;
        }
        if (x < 1 || x > 100) {
            printf("round %d: reject %d\n", round, x);
            continue;
        }
        ++round;
        ++count;
        sum += x;
        printf("round %d: accept %d\n", round, x);
    }

    printf("rounds=%d count=%d sum=%d max=%d\n", round, count, sum, max);
    return 0;
}
```

(1) 先手算应有的结果：有几条输入参与轮次编号，有效成绩几条，总和多少，最高分多少。

(2) 运行程序，贴出输出。两处错误的结果分别由哪一条语句的位置引起？一处让轮次编号错位，另一处让最高分取了不该取的值。

(3) 修好这两处，写出修复后的完整输出。要求 `round` 覆盖每条非结束输入，`max` 只由有效成绩产生。

(4) 修复后若所有输入都无效（例如 `-5 200 -1`），输出里的 `max` 是多少？此时「没有有效成绩」与「最高分为 0」混在一起。给出一种能区分两者的改法：新增哪个对象、在哪些位置更新它。

</Exercise>

<Exercise id="10529" :d="5" :w="3">

写一个库存事件处理器，把 `switch` 里的 `break`、`continue` 与循环的退出放在一起用。

事件从标准输入逐个读入整数：

- `0`：结束程序。
- `1`：入库一件，库存加 1。
- `2`：出库一件，余额减 5 元，售出数加 1。库存为 0 或余额不足 5 元时打印 `rejected`，本条不算成功处理。
- `3`：打印 `stock=<库存> balance=<余额> sold=<售出数>`。
- 其他整数：打印 `unknown event: <数值>`，按未成功处理。

要求：

1. 每成功处理一条事件（1、2、3）后打印 `event <事件号> done`；被拒绝或未知的事件不打印这一行。
2. 连续出现 3 条未成功处理的事件时，打印 `give up` 并结束程序；中间有一条成功事件则重新计数。
3. 初始库存为 2，余额与售出数为 0。
4. 结束时打印 `bye stock=<库存> balance=<余额> sold=<售出数>`。

用下面的输入验收，每行是一个完整的输入序列。

```text
1 2 2 3 9 2 0
```

期望输出：

```text
event 1 done
event 2 done
rejected
stock=1 balance=0 sold=1
event 3 done
unknown event: 9
rejected
bye stock=1 balance=0 sold=1
```

```text
1 2 2 9 8 7 1 0
```

期望输出：

```text
event 1 done
event 2 done
rejected
unknown event: 9
unknown event: 8
give up
bye stock=1 balance=0 sold=1
```

```text
1 2 2 9 1 2 0
```

期望输出：

```text
event 1 done
event 2 done
rejected
unknown event: 9
event 1 done
event 2 done
bye stock=0 balance=0 sold=2
```

最后回答：`give up` 之后要结束的是 `switch` 还是 `while`？为什么不能在那条 `printf` 后面直接写一个 `break`？给出至少两种让循环停下来的写法，并说明各自的清理动作放在哪里。

</Exercise>
