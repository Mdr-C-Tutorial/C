# 5.3 for

`for` 把“初始化、条件判断、步进表达式”集中写在一行，是 C 中最常见的计数循环结构。它与 `while` 可以互相改写，但在边界控制上通常更紧凑。

## 1. 基本语法

```c
for (init; condition; step) {
    statement;
}
```

执行顺序是：先执行 `init`，再判断 `condition`；若条件非零就执行循环体，然后执行 `step`，再回到条件判断。

## 2. 示例

```c
#include <stdio.h>

int main(void) {
    int sum = 0;

    for (int i = 1; i <= 100; ++i) {
        sum += i;
    }

    printf("%d\n", sum);
    return 0;
}
```

输出：

::: terminal

5050

:::

`i` 在 `for` 的初始化部分声明，它的作用域和生存期都仅限于这条 `for` 语句，循环结束后不能再使用 `i`。

## 3. 常见变体

`for (;;)` 表示无限循环；三个表达式都可以省略，但分号必须保留。省略任何一项都应有明确意图，否则会降低可读性。

## 4. 逆序循环的边界

用无符号整数类型（如 `size_t`）的索引逆序遍历时，不能写成 `for (size_t i = n - 1; i >= 0; --i)`：`i >= 0` 恒为真，`i` 为 `0` 时再减 `1` 会回绕到该类型的最大值，循环不会结束，并会越界访问数组。下面这种写法更稳妥：

```c
#include <stddef.h>

void visit_reverse(const int *a, size_t n) {
    for (size_t i = n; i > 0; --i) {
        int v = a[i - 1];
        (void)v;
    }
}
```

它通过 `i > 0` 保证 `i - 1` 总是有效索引，避免把无符号对象减到极大值后继续循环。

## 5. 多控制表达式写法

`for` 还可以在初始化和步进位置维护多个控制对象。这种写法在双指针扫描、窗口移动等场景中很常见，但要保证每个控制对象的职责清晰。

```c
#include <stdio.h>

int main(void) {
    for (int left = 0, right = 4; left <= right; ++left, --right) {
        printf("(%d, %d)\n", left, right);
    }
    return 0;
}
```

输出：

::: terminal

(0, 4)
(1, 3)
(2, 2)

:::

当控制对象过多时，建议提炼出辅助函数，避免把 `for` 头部堆成难读表达式。

## 6. `continue` 与步进表达式

`for` 中执行 `continue` 时，不会直接回到条件判断，而是先执行 `step`，再进入下一轮条件检查。因此，每轮都必须执行的推进动作应放在 `step` 中；若只写在循环体尾部，执行 `continue` 时会被绕过。

## 7. 习题

### 7.1 巩固

<Exercise id="10514" :d="1" :w="2">

先不编译，写出下面程序的输出，再编译运行核对。

```c
#include <stdio.h>

int main(void) {
    int total = 0;

    for (int i = 1; i <= 6; ++i) {
        if (i == 4) {
            continue;
        }
        total += i;
    }
    printf("%d\n", total);

    int n = 0;
    for (;;) {
        n += 3;
        if (n > 8) {
            break;
        }
    }
    printf("%d\n", n);
    return 0;
}
```

1. 两行输出分别是多少？
2. 第一段里 `i == 4` 那一轮，`continue` 之后 `i` 是否还会加一？从结果反推，并说明这与“回到循环体开头”有什么区别。
3. 把 `continue` 换成 `break`，第一行输出变成多少？第二行会受影响吗？
4. 把第一段的 `i <= 6` 改成 `i < 6`，`continue` 版本与 `break` 版本各变成多少？
5. 在循环之后写 `printf("%d\n", i);`，编译结果如何？这说明 `for` 初始化部分声明的对象作用域到哪里为止？

</Exercise>

<Exercise id="10515" :d="2" :w="1">

下面两段循环都能正常编译运行，先写出输出，再核对。

```c
#include <stdio.h>

int main(void) {
    int i = 0;
    while (i < 5) {
        printf("%d ", i);
        i += 2;
    }
    printf("| %d\n", i);

    for (int k = 10; k > 0; k -= 3) {
        printf("%d ", k);
    }
    printf("\n");
    return 0;
}
```

1. 写出两行输出，`|` 后面的值是多少？
2. 把第一段改成等价的 `for`，初始化、条件、步进三段齐全，输出不变。
3. 第二段循环体执行了几次？循环结束后 `k` 是多少？`k` 能在循环外使用吗？
4. 先把第二段的步进改成 `k -= 5`，条件仍是 `k > 0`，输出是什么？再把条件改成 `k >= 0`，输出又多出哪个值？据此说明条件是在每次进入循环体之前求值，还是在之后。
5. 保持步进 `k -= 3`，把条件写成 `k != 0`，程序会怎样？从 10 开始列出前几个 `k`，说明这个条件为什么不可靠。

</Exercise>

<Exercise id="10516" :d="3" :w="2">

下面的函数想把 `n - 1` 到 `0` 依次打印出来，实际却停不下来。

```c
#include <stdio.h>

void countdown(unsigned n) {
    for (unsigned i = n - 1; i >= 0; --i) {
        printf("%u ", i);
    }
    printf("\n");
}

int main(void) {
    countdown(4);
    return 0;
}
```

1. `gcc -std=c17 -Wall -Wextra` 报了什么？抄下原文，指出编译器的推理依据。
2. 运行程序并随即中断，记录开头若干个输出。说明打印到 0 之后为什么接着出现 `4294967295`。
3. 按正文的写法改成 `for (unsigned i = n; i > 0; --i)`，循环体内用 `i - 1`。核对 `countdown(4)` 的输出，再补测 `countdown(0)`，说明后者为什么一次都不打印。
4. 另有写法 `for (unsigned i = n; i-- > 0;)`。循环体里的 `i` 与条件里被判断的值相差多少？用它重写 `countdown`，`countdown(0)` 的表现与第 3 问是否一致？

</Exercise>

### 7.2 编程

<Exercise id="10517" :d="2" :w="2">

写一个程序，读入 `n`（$0 \le n \le 10^5$），按升序输出 1 到 `n` 中不能被 3 整除的数，每行一个，最后单独一行输出这些数的个数。

样例输入：

```in
20
```

样例输出：

```out
1
2
4
5
7
8
10
11
13
14
16
17
19
20
14
```

要求：

1. 循环用 `for`，用 `continue` 跳过 3 的倍数，输出语句放在 `continue` 之后，不要写成 `if (...) { printf(...); }`。
2. 计数用单独的计数器，在输出成功之后加一。
3. 再写一版，把 `continue` 换成 `if` 包裹输出，比较两版输出是否逐字节相同。
4. 第三版在 `continue` 版本的基础上，于循环体末尾再加一条 `++i;`。先预测再运行，记录输出，并解释哪些数被漏掉了。这一问说明什么写法习惯？

</Exercise>

<Exercise id="10518" :d="3" :w="2">

`continue` 在 `for` 里会先执行步进表达式。下面三段程序从不同角度碰到这条规则。

```c
#include <stdio.h>

int main(void) {
    for (int i = 0; i < 6; ++i) {
        if (i == 2) {
            continue;
        }
        printf("%d ", i);
        ++i;
    }
    printf("\n");

    for (int i = 0; i < 6; ++i) {
        if (i == 2) {
            continue;
        }
        printf("%d ", i);
    }
    printf("\n");

    for (int i = 0; i < 6; i += 2) {
        if (i == 2) {
            continue;
        }
        printf("%d ", i);
    }
    printf("\n");
    return 0;
}
```

1. 三段各输出什么？先预测再运行。
2. 第一段为什么漏掉了 1 和 4？去掉循环体里的 `++i` 之后输出变成什么？
3. 第三段为什么只跳过 2，4 却照样打印？如果把步进改成 `++i`、循环体末尾写 `i += 2`，输出变成什么？
4. 把第二段改写成 `while` 循环，要求 `continue` 的语义等价。步进应放在 `continue` 之前还是之后？
5. 综合三段的结论，用一句话说明每轮都必须执行的推进动作应该放在 `for` 的哪个部分，才不会被执行 `continue` 的路径绕过。

</Exercise>

### 7.3 综合与挑战

<Exercise id="10519" :d="4" :w="2" multi>

本页讲了 `for` 头部的三段、双控制对象和逆序边界。下面四问把它们放进同一个题目，每问单独验收。

(1) _[3.2]_ 用双控制对象实现 `void reverse(int *a, size_t n)` 与 `int is_palindrome(const int *a, size_t n)`：前者原地倒置，后者遇到第一对不相等的元素就返回 0。两个函数的 `for` 头部同时维护 `left` 与 `right`，`right` 从 `n` 开始递减，循环体用 `right - 1` 取元素。用下面的 `main` 验证：

```c
int main(void) {
    int a[] = {1, 2, 3, 2, 1};
    int b[] = {1, 2, 2, 1};
    int c[] = {1, 2, 3, 4};
    int d[] = {7};

    printf("%d %d %d %d\n", is_palindrome(a, 5), is_palindrome(b, 4),
           is_palindrome(c, 4), is_palindrome(d, 1));

    reverse(a, 5);
    reverse(c, 4);
    for (size_t i = 0; i < 5; ++i) {
        printf("%d ", a[i]);
    }
    printf("\n");
    for (size_t i = 0; i < 4; ++i) {
        printf("%d ", c[i]);
    }
    printf("\n");
    return 0;
}
```

期望输出：

```text
1 1 0 1
1 2 3 2 1
4 3 2 1
```

再把 `right` 的初值改成 `n - 1`、交换 `a[left]` 与 `a[right]`，分别用 `a`（长度 5）和 `c`（长度 4）测试，结果与上面一致吗？这个改法在 `n` 不为 0 时为什么也对？最后对 `n = 0` 调用一次：用 `printf("%llu\n", (unsigned long long)((size_t)0 - 1))` 确认 `right` 会取到什么值，说明 `0 < SIZE_MAX` 成立后第一次循环会访问哪个下标，并指出这属于什么行为。

(2) _[3.2]_ 阶乘之和 $S_n = \sum_{i=1}^{n} i!$。先写两层循环（外层枚举 `i`，内层从头累乘），再改成一层循环（每轮把上一轮的阶乘乘以 `i`）。验证 `n = 20` 时两版输出都是 `2561327494111820313`，并估算两版在 `n = 20` 时各做多少次乘法。`n = 21` 时输出变成 `-1687962555307394535`，用 `long long` 的范围解释这个值，并说明上界为什么要在写循环之前确定。

(3) _[4.2]_ 下面这段程序把函数调用放进 `for` 的条件：

```c
#include <stdio.h>

static int calls;

static int window(void) {
    ++calls;
    return 4;
}

int main(void) {
    int visited = 0;
    for (int i = 0; i < 10 && window(); ++i) {
        ++visited;
    }
    printf("%d %d\n", visited, calls);
    return 0;
}
```

写出输出，说明 `window()` 被调用了几次、`&&` 的短路在哪一轮起了作用。再把 `window()` 提到循环外存进局部变量，要求 `visited` 不变，而 `calls` 变为 1。最后把条件改成 `i < 10 || window()` 并加一个计数器，预测并验证调用次数的变化，说明为什么把带副作用的调用写在条件里会让循环难以推理。

(4) _[4.2]_ 下面这段循环本来想执行 10 轮：

```c
int count = 0;
for (double x = 0.0; x != 1.0; x += 0.1) {
    ++count;
    if (count > 20) {
        break;
    }
}
printf("%d\n", count);
```

运行后输出 21。解释 `0.1` 在 `double` 中不能精确表示这一点怎样让 `x != 1.0` 永远成立，并打印 `x` 在第 10 轮之后的值作为证据。改成 `for (int k = 0; k < 10; ++k) { double x = k / 10.0; ... }` 后输出 10，说明浮点数为什么不适合做循环控制对象。

</Exercise>
