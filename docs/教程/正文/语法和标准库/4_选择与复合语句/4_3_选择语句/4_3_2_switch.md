# 4.3.2 switch

`switch` 语句是一种多分支选择语句，用于根据值选择不同的执行路径。

## 1. 语法

一般地：

```c
switch (表达式) {
    case 整数常量表达式1:
        语句序列1
        break; // 有时可以没有 break
    case 整数常量表达式2:
        语句序列2
        break; // 有时可以没有 break

    // ... 其他 case
    // ... 其他语句序列
    default:
        默认语句序列
        break;
}
```

其中，`表达式` 必须是整数类型（包括 char、short、int、long 及其无符号版本，以及枚举类型）。

执行 `switch` 语句时，首先计算 `表达式` 的值，然后将其与每个 `case` 标签的值进行比较。如果找到匹配的 `case` 标签，则从相应的标签处开始执行。如果没有找到匹配的 `case` 标签并且存在 `default` 子句，则从 `default` 子句开始执行。

以上面的示例代码为例：

- 如果 `表达式` 的值等于 `整数常量表达式1`，则从 `整数常量表达式1` 这个标签处开始执行，即执行 `语句序列1`，`语句序列2`，`其他语句序列` 和 `默认语句序列`；
- 如果 `表达式` 的值等于 `整数常量表达式2`，则从 `整数常量表达式2` 这个标签处开始执行，即执行 `语句序列2`，`其他语句序列` 和 `默认语句序列`；
- 如果 `表达式` 的值等于其他 `case` 标签后的值，则从相应的标签处开始执行；
- 如果 `表达式` 的值不等于任何一个 `case` 标签的值，则从 `default` 标签开始执行，即执行 `默认语句序列`。

如果在 `switch` 语句内执行时遇到跳转语句 `break;`，则会跳出 `switch` 语句。我们将在 [5.5.1](/教程/正文/语法和标准库/5_循环与跳转/5_5_跳转语句/5_5_1_break_continue) 详细介绍 `break;` 语句的其他功能。

简单来说：**对应 `case` 进，遇到 `break` 出**。

### 1.1 case 标签

`case` 标签用于标识 `switch` 语句中的一个分支。每个 `case` 标签后面跟着一个整数常量表达式。这些表达式不能重复（比如同一个 `switch` 语句中不能存在两个 `case 1:`）。

```c
int a = 1;
const int b = 2;
constexpr int c = 3; /* `constexpr` 用于声明编译期常量，第 15 章详细讲解 */

int value = 3;

switch (a) {
    case a: // 错误：不是常量表达式
    case b: // 错误：即使是 const 对象，也不是常量表达式
    case c: // 正确：使用 constexpr 说明符声明的对象是常量表达式
    case 4: // 正确
    case 2 + 3: // 正确
    case b + 3: // 错误
    case c + 3: // 正确
    case c + 1: // 错误：前面已经有了 case 4:
    default:
}
```

::: details `case` 标签可以在嵌套的复合语句中吗？

可以，但是强烈不推荐。看下面这段代码：

```c
switch (a) {
    case 1:
        printf("到了 case 1 这个标签\n");
        if (val > 0) {
            printf("进入 if 块\n");
    case 2:
            printf("到了 case 2 这个标签\n");
        }
        printf("从 if 块中出来了\n");
        break;
}
```

这种代码**可读性极差**，并且**容易引发逻辑错误**，所以强烈不推荐这种行为。

历史上有个著名的使用**手动循环展开**的编程技巧，用于“高速”地将数据从一个数组复制到另一个数组：

```c
void duffs_device(char *to, char *from, int count) {
    int n = (count + 7) / 8; // 计算需要执行多少轮，每轮处理 8 个字节
    switch (count % 8) { // 计算第一次迭代需要处理的“零头”
        case 0: do { *to++ = *from++;
        case 7:      *to++ = *from++;
        case 6:      *to++ = *from++;
        case 5:      *to++ = *from++;
        case 4:      *to++ = *from++;
        case 3:      *to++ = *from++;
        case 2:      *to++ = *from++;
        case 1:      *to++ = *from++;
                } while (--n > 0);
    }
}
```

这种技巧**是历史的产物**，在当时可能起到加快运行速度的作用。但在**硬件**和**编译器优化**相当发达的今天，**绝不要**使用这种“技巧”。

:::

### 1.2 break 和 fallthrough

`break;` 语句在 `switch` 语句中拥有特殊含义：如果在 `switch` 语句的任何位置执行时遇到跳转语句 `break;`，则会跳出 `switch` 语句。

在大多数场景下，我们希望在执行完某个 `case` 后的语句序列后跳出 `switch` 语句，所以在每个 `case` 后的语句序列末尾都需要添加 `break;` 语句。但有例外：有时候，我们希望在执行完某个 `case` 后的语句序列后继续执行下一个 `case` 后的语句序列，这时不加 `break;` 语句。这种情况被称为 **fallthrough**。

在 C23 标准中，有了[属性](/教程/正文/语法和标准库/15_声明_名称与生存期/15_6_属性)的语法，我们可以在下一个 `case` 标签之前使用 `[[fallthrough]];` 语句来提醒编译器：这个 fallthrough 是有意为之，而不是漏写了 `break;`，从而避免可能的警告。

::: details 可能的警告

在 GCC 中，如果编译选项 `-Wimplicit-fallthrough`（包含于 `-Wextra`）被启用，则会在 fallthrough 处发出警告。

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
            break;
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

### 1.3 default 标签

`default` 标签是可选的，用于处理所有未被 `case` 标签匹配的情况。`default` 标签在每个 `switch` 语句中最多只能出现一次，并且可以放在 `switch` 语句内的任何位置（不一定要在最后）。

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

## 2. 注意事项与应用

### 2.1 作用域问题

`case` 只是标签，无法创造它自己的作用域，所以其内的标识符直到 `switch` 语句结束都是可见的。然而，由于 `switch-case` 语句可能跳过某些语句的执行，可能跳过对象的初始化，后续访问未初始化的对象可能导致未定义行为。

```c
#include <stdio.h>
int main() {
    int i = 2;
    switch (i) {
        case 1:
            int x = 100;
            printf("%d", x);
            break;
        case 2:
            printf("%d", x); // 错误：'x' is used uninitialized 而非 'x' undeclared
            break;
    }
    return 0;
}
```

程序从 `case 2` 直接进入分支，跳过了 `x` 的初始化，随后读取未初始化的 `x`，行为未定义，因此不存在可依赖的输出。另外，标签后直接跟声明（`case 1:` 后的 `int x = 100;`）是 C23 才允许的写法。

很多时候，我们在 `case` 标签末尾加上一个大括号包裹要执行的语句，成为一个复合语句，把标识符的作用范围限制在更小的范围内。

### 2.2 空标签

空的 `case` 标签是合法的，常用于多个值共享同一处理逻辑。例如：

```c
switch (ch) {
    case ' ':
    case '\t':
    case '\n':
        // 处理空白字符
        break;
}
```

## 3. 习题

### 3.1 巩固

<Exercise id="10422" :d="3" :w="3">

下面的程序按字符分类，用 `switch` 给每一类算出一个编号。

```c
#include <stdio.h>

static int kind(char c) {
    int n = 0;
    switch (c) {
        case ' ':
        case '\t':
            n += 1;
        case '\n':
            n += 2;
            break;
        case '0':
        case '1':
            n += 4;
            break;
        default:
            n += 8;
        case '#':
            n += 16;
            break;
    }
    return n;
}

int main(void) {
    printf("%d\n", kind(' '));
    printf("%d\n", kind('\t'));
    printf("%d\n", kind('\n'));
    printf("%d\n", kind('0'));
    printf("%d\n", kind('1'));
    printf("%d\n", kind('x'));
    printf("%d\n", kind('#'));
    return 0;
}
```

(1) 不编译，写出七行输出，并说明 `kind(' ')` 与 `kind('x')` 各自从哪个标签进入、依次执行了哪些语句后才离开 `switch`。

(2) 用 `gcc -std=c17 -Wall -Wextra` 编译并运行，核对输出。把警告抄下来，指出两条 `-Wimplicit-fallthrough` 各对应哪一对相邻标签；`case ' ':` 与 `case '\t':` 这种空标签为什么不会触发警告？

(3) 两处贯穿都是有意写的，但读者未必看得出来。把 `kind` 改写成不依赖贯穿的等价版本：仍然只用 `switch`，用空标签合并同类，每个分支组直接给 `n` 赋值后 `break`。运行确认七个输出完全不变，再说明这种写法与原来的贯穿写法各自的代价。

(4) 回到最初那个程序，把 `default:` 连同它下面的 `n += 8;` 一起移到 `case '#'` 那一组之后，其余不动。先预测 `kind('x')` 与 `kind('#')` 的值，再运行核对，并解释 `default` 的位置为什么会改变结果。此时警告还剩几条？

</Exercise>

<Exercise id="10423" :d="4" :w="3">

下面的程序把各种候选写法都塞进了 `case` 标签，它通不过编译。

```c
#include <stdio.h>

static int probe(int key) {
    const int frozen = 8;
    constexpr int modern = 9;

    switch (key) {
        case 1:
            return 10;
        case 2 + 3:
            return 20;
        case 5:
            return 30;
        case sizeof(int):
            return 40;
        case 'A':
            return 50;
        case frozen:
            return 60;
        case modern:
            return 70;
        case 1.0:
            return 80;
        case key + 1:
            return 90;
        default:
            return -1;
    }
}

int main(void) {
    printf("%d\n", probe(1));
    return 0;
}
```

(1) 用 `gcc -std=c2x -Wall -Wextra` 编译，抄下全部错误，逐条指出它对应哪个 `case`，并说明判据。其中哪些写法本身合法、只是与别的标签撞了值，哪些根本不满足 `case` 标签的要求？

(2) 同一个文件改用 `-std=c17` 编译，多出了哪几条错误？它们都由哪个原因引起？据此说明 `constexpr` 的最低标准版本。

(3) 修改程序，使它在 `-std=c2x -Wall -Wextra` 下编译通过且没有警告，并留下六个分支。`case 1:`、`case 2 + 3:`、`case sizeof(int):`、`case 'A':` 四个标签原样保留（因此 `case 5:` 要去掉，它与 `2 + 3` 撞值）；把 `const int frozen = 8;` 换成 `#define FROZEN 8`，`case frozen:` 相应写成 `case FROZEN:`；`case modern:` 先原样留着，编译后看它到底需不需要改。`1.0` 与 `key + 1` 两条删掉，各用一句话说明这两种想法为什么在 `switch` 里无法表达。

(4) `case 2 + 3:` 与 `case 5:` 的重复在编译期就被发现了，而 `case frozen:` 里的 `frozen` 明明是 `const int` 却不被接受。用「整数常量表达式」这个说法把这两件事讲清楚。

(5) 另写一个小程序，把 `double d = 1.5;` 交给 `switch (d)`，再写一个 `case 1:`。编译诊断是什么？把 `d` 换成 `char` 后能编译吗？据此复述本页第 1 节对 `switch` 表达式类型的要求。

</Exercise>

### 3.2 编程

<Exercise id="10424" :d="4" :w="3">

写一个程序，用 `switch` 完成两件与月份有关的事。

```c
int days_in_month(int year, int month);
void print_season(int month);
```

要求：

1. `days_in_month` 返回该月的天数，`month` 不在 1～12 时返回 -1。闰年 2 月返回 29，闰年判据为「能被 4 整除且不能被 100 整除，或者能被 400 整除」。
2. `print_season` 按 3～5 月打印 `spring`、6～8 月打印 `summer`、9～11 月打印 `autumn`，12 月与 1、2 月打印 `winter`，其余值打印 `unknown`。同一季节的月份用空标签合并。
3. 两个函数都用 `switch` 实现。`days_in_month` 在 `switch` 之前声明 `int days;`，各分支组给它赋值后 `break`，`switch` 之后再 `return days;`，不要在每个分支里直接 `return`。
4. 用 `gcc -std=c17 -Wall -Wextra -pedantic` 编译必须没有警告。

验收：程序输出依次应与下面的两行数字、五行季节名一致（输出用的那几组调用自己写，覆盖同样的输入即可）。

```text
28 29 28 29
31 30 31 -1 -1
winter
spring
summer
autumn
unknown
```

第一行是 `days_in_month` 对 (2023, 2)、(2024, 2)、(1900, 2)、(2000, 2) 的结果，第二行是对 (2024, 1)、(2024, 4)、(2024, 7)、(2024, 0)、(2024, 13) 的结果，后面五行依次是 `print_season(1)`、`print_season(4)`、`print_season(8)`、`print_season(11)`、`print_season(13)`。

完成后做两个对照实验：

(1) 去掉 `days_in_month` 开头的 `int days;`，改成在每个分支组里写 `int days = ...;`，不加括号。编译并抄下诊断，说明为什么两个分支组不能各自声明同名对象。

(2) 给每个分支组加上大括号，重名错误消失了。此时 `switch` 之后那句 `return days;` 还能编译吗？把实际诊断抄下来，说明大括号把 `days` 的作用域限制到了哪里。回到要求 3 的写法，`int days;` 为什么在 `switch` 之后仍然可见？

</Exercise>

### 3.3 综合与挑战

<Exercise id="10425" :d="5" :w="4" multi>

本节 `details` 里给出的 Duff's device 用 `switch` 跳进一个 `do` 循环的中间。下面的程序把同一个结构用在计数上：`countdown(count)` 从 `count - 1` 倒数到 0，`switch` 的标签决定第一轮从倒数第几个数开始。

```c
#include <stdio.h>

static void countdown(int count) {
    int n = (count + 7) / 8;
    switch (count % 8) {
        case 0: do { printf("%d ", --count);
        case 7:      printf("%d ", --count);
        case 6:      printf("%d ", --count);
        case 5:      printf("%d ", --count);
        case 4:      printf("%d ", --count);
        case 3:      printf("%d ", --count);
        case 2:      printf("%d ", --count);
        case 1:      printf("%d ", --count);
                } while (--n > 0);
    }
}

int main(void) {
    printf("0: ");
    countdown(0);
    printf("\n1: ");
    countdown(1);
    printf("\n3: ");
    countdown(3);
    printf("\n4: ");
    countdown(4);
    printf("\n8: ");
    countdown(8);
    printf("\n9: ");
    countdown(9);
    printf("\n");
    return 0;
}
```

(1) _[3.2]_ 编译运行，抄下六行输出。哪个 `count` 的输出不对？从 `count % 8` 的值、`n` 的初值和 `do` 循环的判断位置三处说明原因。再在 `main` 里补上 2、5、6、7 四次调用，确认其余的 `count` 都正确。

(2) _[5.3]_ 最省事的修法是在 `case 0:` 后面加一句 `break;`，让 `count % 8 == 0` 时直接离开 `switch`。写上这一句再编译运行，`0:` 修好了，`8:` 那一行变成了什么？为什么 `count = 8` 也一起坏了？给出正确的修法，要求 `countdown(0)` 不输出任何数字，其余 `count` 仍输出 `count` 个数，并且不给 `switch` 增加新的标签。

(3) _[4.4]_ 用 `-Wall -Wextra` 编译时，相邻 `case` 之间共报出七条 `-Wimplicit-fallthrough`，正好是 0→7、7→6、…、2→1 这七对。把每一处都标成 C23 的 `[[fallthrough]];`，分别在 `-std=c17`、`-std=c17 -pedantic`、`-std=c2x`、`-std=c2x -pedantic` 下编译，记下四次警告的条数。再把标记换成注释 `/* fall through */`，同样编译四次。哪一列数字说明了「C17 项目该选注释」？

(4) _[4.4*]_ 用普通的循环重写 `countdown`，功能不变（第 5 章才正式讲循环，本页 1.2 节的示例里已经出现过 `for`，可以照着写）。对比两个版本对 0～9 的输出，并从「编译器优化的能力」和「人读代码的代价」两方面说明今天为什么不该再写这种代码。

</Exercise>
