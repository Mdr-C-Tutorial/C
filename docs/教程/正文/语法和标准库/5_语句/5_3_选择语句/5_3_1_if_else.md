# if else

if-else 用于根据条件执行不同的语句。

## 语法

1. `if (condition) 语句1`：如果 `condition` 成立（求值之后不等于 `0`），执行 `语句1`。
2. `if (condition) 语句1 else 语句2`：如果 `condition` 成立（不等于 `0`），执行 `语句1`；否则，执行 `语句2`。

其中的 `condition` 是标量类型的表达式（毕竟要和整数 `0` 做比较）；`语句1` 和 `语句2` 可以是任意语句，包括其他的 `if-else` 语句。也就是说，`if-else` 语句可以嵌套使用，如下所示：

```c
if (condition1)
    语句1
else if (condition3)
    语句2
else 语句3
```

在上面的代码中：

- 如果 `condition1` 成立（不等于 `0`），执行 `语句1`。
- 如果 `condition1` 不成立（等于 `0`）：
  - 如果 `condition2` 成立（不等于 `0`），执行 `语句2`。
  - 否则，执行 `语句3`。

在大多数情况下，满足某个条件时需要执行的语句多于一个，此时 `if` 和 `else` 后面可以跟随一条复合语句（用大括号包裹的语句和声明序列），这样就可以对很多个语句进行选择性执行了。
如下所示：

```c
if (condition) {
    语句1
    语句2
} else if (condition){
    语句3
    语句4
} else {
    语句5
}
```

## 应用和注意事项

### 1. 简单示例

判断一个数是否为正数（只用 `if` 即可），用到 `%` 运算符：

```c
int num = 10;
if (num > 0){
    printf("num 是正数\n");
}
```

判断一个整数是否为偶数（使用 `if-else` 即可）：

```c
int num = 10;
if (num % 2 == 0){
    printf("num 是偶数\n");
} else {
    printf("num 是奇数\n");
}
```

判断一个整数和零的关系（需要使用 `if - else if - else`）：

```c
int num = 10;
if (num > 0) {
    printf("num 是正数\n");
} else if (num == 0) {
    printf("num 是零\n");
} else {
    printf("num 是负数\n");
}
```

根据分数划定等级（假如大等于 85 分是 A，大等于 70 分是 B，大等于 60 分是 C，低于 60 分是 D）：

```c
#include <stdio.h>

int main(void) {
    int score;
    printf("请输入分数：");
    scanf("%d", &score);
    if (score >= 85) {
        printf("A\n");
    } else if (score >= 70) {
        printf("B\n");
    } else if (score >= 60) {
        printf("C\n");
    } else {
        printf("D\n");
    }
}
```

### 减少缩进层数

当代码中包含多层 `if-else` 嵌套时，会形成所谓的“箭头代码”（Arrow Code），因为缩进看起来像一个箭头 `>`：

```c
if (condition1){
    if (condition2){
        if (condition3){
            if (condition4){
                if (condition5){
                    if (condition6){
                        // ... 可能很长
                    }
                }
            }
        }
    }
}
```

这种代码结构复杂，难以阅读和维护。（我亲眼见过 8 层嵌套的 `if-else`）

一种非常有效的优化方法是使用**卫语句（Guard Clauses）**。

卫语句的核心思想是：**在函数或代码块的开头，先检查并处理所有不符合条件的、异常的或边界的情况，然后立即退出（例如使用 `return`）**。这样，函数的主体部分就可以在没有嵌套的情况下平铺直叙地编写。

优化前：深层嵌套的“箭头代码”：

假设我们有一个函数，根据输入的分数（0-100 分）给出评语。我们需要先判断分数是否有效。

```c
#include <stdio.h>

void print_grade_comment(int score) {
    // 1. 先判断分数是否在有效范围内
    if (score >= 0 && score <= 100) {  // 第一层缩进
        // 2. 如果有效，再根据具体分数给出评语
        if (score >= 90) {             // 第二层缩进
            printf("非常优秀！\n");
        } else if (score >= 60) {
            printf("做得不错，通过了。\n");
        } else {
            printf("需要更加努力。\n");
        }
    } else {
        printf("错误：分数必须在 0 到 100 之间。\n");
    }
}
```

在这段代码里，核心的评语逻辑被包裹在一个 `if` 语句块中，增加了缩进，使得代码逻辑不是那么一目了然。

我们反过来想：先把所有“坏”情况（无效分数）处理掉，并立即退出函数。

```c
#include <stdio.h>

void print_grade_comment_improved(int score) {
    // 卫语句：检查无效分数，如果无效则直接报错并退出函数
    if (score < 0 || score > 100) {
        printf("错误：分数必须在 0 到 100 之间。\n");
        return; // 提前退出
    }

    // 如果代码能执行到这里，说明分数一定是有效的。
    if (score >= 90) {
        printf("非常优秀！\n");
    } else if (score >= 60) {
        printf("做得不错，通过了。\n");
    } else {
        printf("需要更加努力。\n");
    }
}
```

### 紧接复合语句

即使满足某个条件时只需执行一条表达式语句，也建议使用大括号将其包裹，使其使用复合语句：这样不仅容易阅读（方便读出由 `if` 控制的范围），而且方便后续的扩展，避免出现意料之外的错误（如悬垂 if 等）。

::: details 悬垂 if

观察下面的代码：

```c
if (condition1)
    if (condition2)
        语句1;
else
    语句2;
```

问题来了：上面的 `else` 和哪个 `if` 相对应？

不是第一个，而是第二个。别被 _缩进_ 忽悠了。根据规则，`else` 是和最近的未配对的 `if` 相对应的。（这个规则是构建抽象语法树 (Abstract Syntax Tree, AST) 时消除歧义 (Grammar Ambiguity) 的约定，符合直觉，容易实现）

ps. 有些学校的期末考试题目也会在这里挖坑（ :(

:::

::: details 一个和悬垂 if 很像的错误

假如我有如下功能：当角色生命值等于 `0` 时，将角色的状态设置为死亡。如果不使用大括号包裹，我可能会写出这样的代码：

```c
if (hp == 0)
    state = PLAYER_DEAD;
if ( /* 其他条件 */ )
    // 其他代码
```

现在，如果我想增加功能：在角色死亡后，打印一条消息。那么我可能会这样写：

```c
if (hp == 0)
    state = PLAYER_DEAD;
    printf("角色死亡\n");
if ( /* 其他条件 */ )
    // 其他代码
```

但是，当运行的时候，我会发现，无论角色生命值是否等于 0，都会打印消息。这是因为，`if` 语句只能控制其后的**第一条语句**即 `state = PLAYER_DEAD;` 的执行与否，因此即使角色生命值不等于 0，`printf("角色死亡\n");` 也会被执行。

为了避免这种情况，建议永远使用大括号包裹 `if` 之后的语句，使之成为复合语句，如下所示：

```c
if (hp == 0) {
    state = PLAYER_DEAD;
}
if ( /* 其他条件 */ ) {
    // 其他代码
}
```

这样的话，扩展功能之后，就不会影响到原来的代码了：

```c
if (hp == 0) {
    state = PLAYER_DEAD;
    printf("角色死亡\n");
}
if ( /* 其他条件 */ )
    // 其他代码
```

这个问题曾经导致了苹果的一次严重的安全漏洞：[**The Apple ‘goto fail;’ vulnerability**](https://www.blackduck.com/blog/understanding-apple-goto-fail-vulnerability-2.html)

:::
