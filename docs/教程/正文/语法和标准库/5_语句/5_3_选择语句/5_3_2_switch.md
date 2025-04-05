# switch

`switch` 语句是一种多分支选择语句，用于根据值选择不同的执行路径。

## 语法

```c
switch (表达式) {
    case 整数常量表达式1:
        语句序列1
    case 整数常量表达式2:
        语句序列2
    // ... 其他 case
    // ... 其他语句序列
    default:
        默认语句序列
        break;
}
```

其中，`表达式` 必须是整数类型。

执行 `switch` 语句时，首先计算 `表达式` 的值，然后将其与每个 `case` 标签的值进行比较。如果找到匹配的 `case` 标签，则从相应的标签处开始执行。如果没有找到匹配的 `case` 标签，则从 `default` 子句开始执行。

以上面的示例代码为例：

- 如果 `表达式` 的值等于 `整数常量表达式1`，则从 `整数常量表达式1` 这个标签处开始执行，即执行 `语句序列1`，`语句序列2`，`其他语句序列` 和 `默认语句序列`；
- 如果 `表达式` 的值等于 `整数常量表达式2`，则从 `整数常量表达式2` 这个标签处开始执行，即执行 `语句序列2`，`其他语句序列` 和 `默认语句序列`；
- 如果 `表达式` 的值等于其他 `case` 标签后的值
- 如果 `表达式` 的值不等于任何一个 `case` 标签的值，则从 `default` 标签开始执行，即执行 `默认语句序列`。

如果在 `switch` 语句内执行时遇到跳转语句 `break;`，则会跳出 switch 语句。我们将在 [5.4.1](/教程/正文/语法和标准库/5_语句/5_4_跳转语句/5_4_1_break_continue.md) 详细介绍 `break;` 语句的其他功能。

### case 标签

### break 和 fallthrough

`break;` 语句在 `switch` 语句中拥有特殊含义：如果在 `switch` 语句的任何位置执行时遇到跳转语句 `break;`，则会跳出 `switch` 语句。

在大多数场景下，我们希望在执行完某个 `case` 后的语句序列后跳出 `switch` 语句，所以在每个 `case` 后的语句序列末尾都需要添加 `break;` 语句。但有例外：有时候，我们希望在执行完某个 `case` 后的语句序列后继续执行下一个 `case` 后的语句序列，这时不加 `break;` 语句。这种情况被称为 **fallthrough**。

在 C23 标准中，有了[属性](/教程/正文/语法和标准库/9_对象/9_6_属性.md)的语法，我们可以在 `case` 标签后使用 `[[fallthrough]];` 语句来提醒编译器：这个 fallthrough 是有意为之，而不是漏写了 `break;`，从而避免可能的警告。

::: details 可能的警告

在 GCC 中，如果编译选项 `-Wimplicit-fallthrough`（包含于 `-WExtra`）被启用，则会在 fallthrough 处发出警告。

:::

示例：

```c
#include <stdio.h>

void func(int x) {
    if (x != 1) {
        putchar('\n');
    }
    printf("func(%d): ", x);
    switch (x) {
        case 1:
            printf("case 1.");
            [[fallthrough]];
        case 2:
            printf("case 2.");
            [[fallthrough]];
        case 3:
            printf("case 3.");
            break;
        case 4:
            printf("case 4.");
            [[fallthrough]];
        case 5:
            [[fallthrough]];
        case 6:
            printf("case 6.");
    }
}

int main(void) {
    for (int i = 1; i < 7; ++i) func(i);
}
```

可能的输出：

```ansi
func(1): case 1.case 2.case 3.
func(2): case 2.case 3.
func(3): case 3.
func(4): case 4.case 6.
func(5): case 6.
func(6): case 6.
```

### default 标签

```c
#include <stdio.h>

void func(int x) {
    if (x != 1) {
        putchar('\n');
    }
    printf("func(%d): ", x);
    switch (x) {
        case 1:
            printf("case 1.");
            break;
        default:
            printf("default.");
            [[fallthrough]];
        case 2:
            printf("case 2.");
    }
}

int main(void) {
    for (int i = 1; i < 4; ++i) func(i);
}
```

可能的输出：

```ansi
func(1): case 1.
func(2): case 2.
func(3): default.case 2.
```

## 应用

`switch` 语句根据不同的条件执行不同的操作，经常用于有限状态自动机。

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

::: warning 不适用的情况

当今市面上的一些教程在讲 `switch` 语句的时候，使用的一些示例代码中的情境实际上是不适用 `switch` 语句去处理的。具体地，`switch` 语句只适用于：

- 有限种状态（尤其是[枚举](/教程/正文/语法和标准库/8_枚举.md)）
- 每种状态的代码逻辑不同

的情况。如果不满足以上两点，很可能有更适合的处理方法。下面举两个例子：

**1. 成绩等级：**

教程中可能存在的不适用的代码：

```c
#include <stdio.h>
int main() {
    int score = 0;
    printf("请输入成绩：");
    scanf("%d", &score);
    switch (score / 10) {
        case 10:
        case 9:
            printf("成绩等级：A\n");
            break;
        case 8:
            printf("成绩等级：B\n");
            break;
        case 7:
            printf("成绩等级：C\n");
            break;
        case 6:
            printf("成绩等级：D\n");
            break;
        default:
            printf("成绩等级：E\n");
            break;
    }
    return 0;
}
```

更合适的写法（使用 `if-else` 语句）：

```c
#include <stdio.h>

int main() {
    int score = 0;
    printf("请输入成绩：");
    scanf("%d", &score);
    if (score >= 90) {
        printf("成绩等级：A\n");
    } else if (score >= 80) {
        printf("成绩等级：B\n");
    } else if (score >= 70) {
        printf("成绩等级：C\n");
    } else if (score >= 60) {
        printf("成绩等级：D\n");
    } else {
        printf("成绩等级：E\n");
    }
    return 0;
}
```

**2. 简单的菜单选择：**

教程中可能存在的不适用的代码：

```c
#include <stdio.h>

// 省略函数声明...

int main() {
    int choice = 0;
    printf("请选择操作：\n");
    printf("1. 显示菜单\n");
    printf("2. 添加学生\n");
    printf("3. 删除学生\n");
    printf("4. 查询学生\n");
    scanf("%d", &choice);
    switch (choice) {
        case 1:
            printf("显示菜单\n");
            break;
        case 2:
            printf("添加学生\n");
            break;
        case 3:
            printf("删除学生\n");
            break;
        case 4:
            printf("查询学生\n");
            break;
        default:
            printf("无效的选择\n");
            break;
    }
    return 0;
}
```

更合适的写法（使用**表驱动**，涉及到函数指针）：

```c
#include <stdio.h>

void show_menu(void){ /* 一些操作 */ }
void add_student(void){ /* 一些操作 */ }
void delete_student(void){ /* 一些操作 */ }
void query_student(void){ /* 一些操作 */ }

static void (*menu[])(void) = {
    show_menu,
    add_student,
    delete_student,
    query_student,
};

int main() {
    int choice = 0;
    printf("请选择操作：\n");
    printf("1. 显示菜单\n");
    printf("2. 添加学生\n");
    printf("3. 删除学生\n");
    printf("4. 查询学生\n");
    scanf("%d", &choice);
    if(choice >= 1 && choice <= 4){
        menu[choice - 1]();
    } else {
        printf("无效的选择\n");
    }
    return 0;
}
```

:::