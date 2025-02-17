# switch

switch 语句是一种多分支选择语句，用于根据值选择不同的执行路径。

## 语法

```c
switch (表达式) {
    case 常量表达式1:
        语句序列1
    case 常量表达式2:
        语句序列2
    // ... 其他 case
    // ... 其他语句序列
    default:
        默认语句序列
        break;
}
```

执行 switch 语句时，首先计算 `表达式` 的值，然后将其与每个 case 标签的值进行比较。如果找到匹配的 case 标签，则从相应的标签处开始执行。如果没有找到匹配的 case 标签，则从 default 子句开始执行。

以上面的示例代码为例：

- 如果 `表达式` 的值等于 `常量表达式1`，则从 `常量表达式1` 这个标签处开始执行，即执行 `语句序列1`，`语句序列2`，`其他语句序列` 和 `默认语句序列`；
- 如果 `表达式` 的值等于 `常量表达式2`，则从 `常量表达式2` 这个标签处开始执行，即执行 `语句序列2`，`其他语句序列` 和 `默认语句序列`；
- 如果 `表达式` 的值等于其他 `case` 标签后的值
- 如果 `表达式` 的值不等于任何一个 case 标签的值，则从 `default` 标签开始执行，即执行 `默认语句序列`。

如果在 `switch` 语句内执行时遇到跳转语句 `break;`，则会跳出 switch 语句。我们将在 [5.4.1](/教程/正文/语法和标准库/5_语句/5_4_跳转语句/5_4_1_break_continue.md) 详细介绍 `break;` 语句的其他功能。

### case 标签

### break 和 fallthrough

`break;` 语句在 `switch` 语句中拥有特殊含义：如果在 `switch` 语句的任何位置执行时遇到跳转语句 `break;`，则会跳出 switch 语句。

### default 子句

## 应用

switch 语句根据不同的条件执行不同的操作，经常用于有限状态自动机。

状态机示例：

```c
#include <stdio.h>

int main() {
    int state = 1;  // 初始状态：等待投币
    int money = 0;  // 投入的钱
    int choice = 0; // 选择的饮料

    printf("欢迎使用自动售货机！\n");

    while (1) {
        switch (state) {
            case 1:  // 等待投币状态
                printf("请投币（1-10 元）：");
                scanf("%d", &money);
                if (money > 0 && money <= 10) {
                    printf("收到 %d 元\n", money);
                    state = 2;
                } else {
                    printf("投币金额无效\n");
                }
                break;

            case 2:  // 选择饮料状态
                printf("请选择饮料（1-可乐 3 元，2-雪碧 3 元）：");
                scanf("%d", &choice);
                if (choice == 1 || choice == 2) {
                    if (money >= 3) {
                        state = 3;
                    } else {
                        printf("金额不足，退币中...\n");
                        state = 1;
                    }
                } else {
                    printf("选择无效\n");
                }
                break;

            case 3:  // 出货状态
                printf("正在出货...\n");
                money -= 3;
                state = 4;
                break;

            case 4:  // 找零状态
                if (money > 0) {
                    printf("找零 %d 元\n", money);
                }
                printf("交易完成！\n");
                state = 1;   // 重置为等待投币状态
                money = 0;   // 重置投币金额
                choice = 0;  // 重置选择
                break;

            default:
                printf("系统错误\n");
                return 1;
        }
    }

    return 0;
}
```
