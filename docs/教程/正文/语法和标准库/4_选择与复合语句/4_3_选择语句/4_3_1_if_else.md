# 4.3.1 if else

if-else 用于根据条件执行不同的语句。

## 1. 语法

1. `if (condition) 语句1`：如果 `condition` 成立（求值之后不等于 `0`），执行 `语句1`。
2. `if (condition) 语句1 else 语句2`：如果 `condition` 成立（不等于 `0`），执行 `语句1`；否则，执行 `语句2`。

其中的 `condition` 是标量类型的表达式（毕竟要和整数 `0` 做比较）；`语句1` 和 `语句2` 可以是任意语句，包括其他的 `if-else` 语句，因此 `if-else` 语句可以嵌套使用，如下所示：

```c
if (condition1)
    语句1
else if (condition2)
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

## 2. 应用和注意事项

### 2.1 简单示例

判断一个数是否为正数（只用 `if` 即可）：

```c
int num = 10;
if (num > 0){
    printf("num 是正数\n");
}
```

输出：

::: terminal

num 是正数

:::

判断一个整数是否为偶数（使用 `if-else` 即可），用到 `%` 运算符：

```c
int num = 10;
if (num % 2 == 0){
    printf("num 是偶数\n");
} else {
    printf("num 是奇数\n");
}
```

输出：

::: terminal

num 是偶数

:::

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

输出：

::: terminal

num 是正数

:::

根据分数划定等级（假如大于等于 85 分是 A，大于等于 70 分是 B，大于等于 60 分是 C，低于 60 分是 D）：

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

例如输入 `85` 时，交互结果为：

::: terminal

请输入分数：85
A

:::

### 2.2 减少缩进层数

当代码中包含多层 `if-else` 嵌套时，缩进会连成箭头 `>` 的形状，这种写法称为“箭头代码”（Arrow Code）：

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

卫语句的做法是：**在函数或代码块的开头，先检查并处理所有不符合条件的、异常的或边界的情况，然后立即退出（例如使用 `return`）**。这样，函数的主体部分就可以在没有嵌套的情况下平铺直叙地编写。

假设我们有一个函数，根据输入的分数（0-100 分）给出评语。我们需要先判断分数是否有效。

::: code-group

```c [嵌套写法]
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

```c [卫语句]
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

:::

第一段把核心评语逻辑包裹在额外的 `if` 块中。第二段先处理无效分数并立即退出，使正常路径不再增加一层缩进。

### 2.3 紧接复合语句

即使满足某个条件时只需执行一条表达式语句，也建议使用大括号将其包裹，使其成为复合语句：这样容易读出由 `if` 控制的范围，也方便后续扩展，避免出现意料之外的错误（如悬垂 if 等）。

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

`else` 和第二个 `if` 配对。别被 _缩进_ 忽悠了。根据规则，`else` 是和最近的未配对的 `if` 相对应的。（这个规则是构建抽象语法树 (Abstract Syntax Tree, AST) 时消除歧义 (Grammar Ambiguity) 的约定，符合直觉，容易实现）

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
if ( /* 其他条件 */ ) {
    // 其他代码
}
```

这个问题曾经导致了苹果的一次严重的安全漏洞：[**The Apple ‘goto fail;’ vulnerability**](https://www.blackduck.com/blog/understanding-apple-goto-fail-vulnerability-2.html)

:::

## 3. 习题

### 3.1 巩固

<Exercise id="10418" :d="1" :w="1">

下面四个函数都用了 `if`，但有的条件是多余的，有的条件写错了。

```c
static int sign_of(int n) {
    if (n > 0) {
        return 1;
    }
    if (n < 0) {
        return -1;
    }
    return 0;
}

static int is_even(int n) {
    if (n % 2 == 1) {
        return 0;
    }
    return 1;
}

static int in_range(int value, int lo, int hi) {
    if (value >= lo || value <= hi) {
        return 1;
    }
    return 0;
}

static int is_upper(char ch) {
    if (ch >= 'A') {
        if (ch <= 'Z') {
            return 1;
        }
    }
    return 0;
}
```

1. `sign_of` 的三个分支分别覆盖哪些输入？最后一个 `return 0` 有没有更直接的写法？
2. `is_even(-3)` 返回什么？`-3 % 2` 等于多少？`is_even` 对哪些输入会给出错误答案？用 `if (n % 2 != 0)` 改写，并说明为什么这样写对正负数都成立。
3. `in_range(0, 1, 10)` 返回什么？`in_range(100, 1, 10)` 呢？把 `||` 改成正确的运算符，再给出 `is_upper` 那种两层嵌套的等价写法。
4. `is_upper` 的两层 `if` 可以合并成一个条件。合并后与原来的可读性相比如何？什么时候分层写更清楚？
5. 四个函数里哪些 `if` 是卫语句，哪些是普通分支？按 4.3.1 节 2.2 小节的说法，`sign_of` 改成卫语句后代码是变短了还是变长了？

</Exercise>

<Exercise id="10419" :d="2" :w="2">

下面的程序判断三角形，但条件顺序有问题。

```c
#include <stdio.h>

/* 返回 3：等边；2：等腰；1：不等边；0：不能构成三角形；-1：边长非法 */
static int classify_triangle(int a, int b, int c) {
    if (a <= 0 || b <= 0 || c <= 0) {
        return -1;
    }
    if (a == b || b == c || a == c) {
        return 2;
    }
    if (a + b <= c || b + c <= a || a + c <= b) {
        return 0;
    }
    if (a == b && b == c) {
        return 3;
    }
    return 1;
}

int main(void) {
    printf("%d %d %d %d %d %d %d\n",
           classify_triangle(3, 3, 3), classify_triangle(3, 3, 4),
           classify_triangle(3, 4, 5), classify_triangle(1, 2, 3),
           classify_triangle(0, 4, 5), classify_triangle(2, 2, 5),
           classify_triangle(-1, 4, 5));
    return 0;
}
```

1. 写出七个数。
2. `classify_triangle(3, 3, 3)` 本该返回 3，实际返回 2。哪个条件先满足了？说明 `if` 链的判定顺序怎样影响结果。
3. `classify_triangle(1, 2, 3)` 返回什么？三边满足三角形不等式吗？
4. 调整条件的顺序，使七个结果依次为 `3 2 1 0 -1 0 -1`。只能移动现有的 `if`，不能改变任何一个条件的写法。
5. 调整后的顺序是否唯一？说明为什么「等边」必须排在「等腰」之前，而「三边合法」又必须排在两者之前。
6. 用 `else if` 改写调整后的链，输出不变。改用 `else if` 之后，某个条件的真假还依赖前面条件的结果吗？

</Exercise>

### 3.2 编程

<Exercise id="10420" :d="4" :w="3">

下面的函数用六层嵌套处理下单，逻辑正确但难以阅读。

```c
#include <stdio.h>

/* 返回应收金额（分）；库存或数量不合法时返回 -1，
   并在返回前打印一行 settled 记录。 */
static int checkout(int stock, int qty, int coupon) {
    int total = 0;

    if (stock > 0) {
        if (qty > 0) {
            if (qty <= stock) {
                total = qty * 100;
                if (coupon > total) {
                    coupon = total;
                }
                total -= coupon;
            } else {
                total = -1;
            }
        } else {
            total = -1;
        }
    } else {
        total = -1;
    }

    printf("settled: %d\n", total);
    return total;
}

int main(void) {
    printf("= %d\n", checkout(0, 1, 0));
    printf("= %d\n", checkout(5, 0, 0));
    printf("= %d\n", checkout(5, 6, 0));
    printf("= %d\n", checkout(5, 2, 0));
    printf("= %d\n", checkout(5, 2, 50));
    printf("= %d\n", checkout(5, 2, 500));
    return 0;
}
```

1. 写出十二行输出（`settled` 与 `=` 交替）。
2. 用卫语句改写三个失败分支，主路径最多两层缩进。要求函数结尾那行 `settled` 记录在任何输入下都照常打印。
3. 直接在每个卫语句里写 `return -1;` 会出什么问题？用原程序的测试用例验证，指出哪几行少了。
4. 给出两种保留尾部的改法：一种把所有失败情况压成一条 `if` 链；另一种用 `goto` 跳到函数末尾的记录处（`goto` 的用法见 [5.5.2](/教程/正文/语法和标准库/5_循环与跳转/5_5_跳转语句/5_5_2_goto)，现在只需照抄结构）。两者的输出必须与原程序完全一致。
5. 比较两种改法：分支数、缩进层数、新增的标号各是多少？如果以后要在失败时再加一条 `printf`，哪种改法改一处就够？

</Exercise>

### 3.3 综合与挑战

<Exercise id="10421" :d="5" :w="4">

下面这段告警判定编译通过，但输出不对。

```c
#include <stdio.h>

/* 返回告警等级：3 严重，2 警告，1 注意，0 正常，-1 参数错误 */
static int alarm_level(int temp, int pressure, int door_open) {
    int level = 0;

    if (temp < -40 || temp > 125) {
        return -1;
    }
    if (pressure < 0 || pressure > 200) {
        return -1;
    }

    if (temp > 100 || pressure > 150);
    {
        level = 3;
    }

    if (temp > 80 || pressure > 120) {
        level = 2;
    }

    if (door_open)
        if (level == 0)
            level = 1;
        else
            level = 0;

    return level;
}

int main(void) {
    printf("%d %d %d %d %d %d %d %d\n",
           alarm_level(20, 100, 0), alarm_level(20, 100, 1),
           alarm_level(85, 100, 0), alarm_level(20, 130, 0),
           alarm_level(101, 100, 0), alarm_level(20, 151, 1),
           alarm_level(-41, 100, 0), alarm_level(20, 201, 0));
    return 0;
}
```

1. 用 `-Wall -Wextra` 编译，抄下全部诊断（三条警告外加一条 note）。它们分别指向哪三处写法？
2. 写出八个返回值。逐项对照：温度 20、压力 100、门关着时本应返回 0，实际返回什么？门开着、其余正常时本应返回 1，实际呢？
3. 逐一说明这三处写法各自错在哪：`if (...);` 后面跟块、连续的 `if` 覆盖、以及没有花括号的 `if` 嵌套。第三处里 `else` 实际属于哪个 `if`？
4. 修复三处缺陷，并把重复的阈值判定改成 `else if` 链。要求八个输出依次为 `0 1 2 2 3 3 -1 -1`。
5. 修复后，`level` 的赋值路径一共几条？画一张 `temp`、`pressure`、`door_open` 到 `level` 的判定表（可以只列边界值），说明每条路径都能落到唯一一个等级。
6. 这道题的三处缺陷，`-Wall -Wextra` 都给出了警告。把这段代码当作一次代码评审的材料：如果团队把警告当作错误（`-Werror`）处理，这三个缺陷能否进入仓库？

</Exercise>
