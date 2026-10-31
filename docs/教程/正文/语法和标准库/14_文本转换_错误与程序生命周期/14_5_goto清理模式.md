# 14.5 goto 与统一清理

第 5 章已经介绍 `goto` 的基本语义。取得多个资源后，失败路径可能需要按相反顺序释放已经取得的资源；这时可以把清理出口集中在函数末尾。

## 1. 典型清理模式

```c
#include <stdio.h>
#include <stdlib.h>

int work(void) {
    FILE *fp = NULL;
    int *buf = NULL;
    int rc = -1;

    fp = fopen("data.txt", "r");
    if (fp == NULL) {
        goto out;
    }

    buf = malloc(1024 * sizeof *buf);
    if (buf == NULL) {
        goto out;
    }

    rc = 0;

out:
    free(buf);
    if (fp != NULL) {
        fclose(fp);
    }
    return rc;
}
```

这类写法比多层 `if` 嵌套更容易保证“每条失败路径都能完整释放资源”。

## 2. 与可变修改类型的约束

`goto` 不能从某个作用域外部跳入声明了可变修改类型对象（例如变长数组 `int a[n];`）的内部作用域。遇到这类结构时，应改写控制流或调整作用域，而不是强行跳入。

## 3. 清理标签应保持幂等

统一失败出口通常会执行“可能已经执行过，也可能还没执行”的释放逻辑，因此标签后的清理代码应尽量写成幂等形式：`free(NULL)` 允许直接调用，文件句柄在关闭前先判空，状态位在释放后及时复位。这样一来，不同失败路径复用同一标签时更不容易出现二次释放或漏释放。

```c
#include <stdio.h>
#include <stdlib.h>

int load_all(void) {
    FILE *fp = NULL;
    char *buf = NULL;
    int rc = -1;

    fp = fopen("a.txt", "rb");
    if (fp == NULL) {
        goto out;
    }

    buf = malloc(4096);
    if (buf == NULL) {
        goto out;
    }

    rc = 0;

out:
    free(buf);
    if (fp != NULL) {
        fclose(fp);
    }
    return rc;
}
```

## 4. 习题

<Exercise id="11418" :d="2" :w="1">

以第 1 节的 `work()` 为例回答：

1. `fopen` 失败时，函数返回什么？执行了哪些释放动作？
2. `fopen` 成功而 `malloc` 失败时，函数返回什么？执行了哪些释放动作？
3. 两次取得资源的调用都成功时，函数返回什么？
4. 把 `int rc = -1;` 改成 `int rc = 0;`，哪些路径的行为会出错？调用方看到的是什么？
5. `work()` 的出口先 `free(buf)`，再判 `fp` 是否为空。若 `fopen` 失败直接跳到出口，`buf` 与 `fp` 各是什么值？为什么 `free(buf)` 不需要判空，而 `fclose(fp)` 必须判空？

</Exercise>

<Exercise id="11419" :d="2" :w="2">

下面的函数想用统一清理，但清理代码不能应付所有失败路径：

```c
#include <stdio.h>
#include <stdlib.h>

int load(const char *path, size_t size) {
    int rc = -1;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        goto out;
    }

    char *buf = malloc(size);
    if (buf == NULL) {
        goto out;
    }

    rc = 0;

out:
    free(buf);
    fclose(fp);
    return rc;
}
```

(1) 分别说明 `fopen` 失败、`malloc` 失败两条路径执行到 `out:` 时 `buf` 与 `fp` 的值。哪条路径上有未定义行为，共有几处？另一条路径为什么没有？

(2) 用 `-std=c17 -Wall -Wextra` 编译，记下警告；如果你用的是 GCC，再加 `-Wjump-misses-init` 试一次。运行程序：`load("no_such_file", 64)` 在你的实现上可能直接崩溃，也可能若无其事地跑完。说明为什么后一种情况不能当作“代码没错”的证据。

(3) 按本节的写法修正：把两个指针都在函数开头初始化为 `NULL`，让清理代码只依赖这两个初值。贴出修正后的完整函数，并说明修正后 `free` 与 `fclose` 各自是否需要判空。

</Exercise>

<Exercise id="11420" :d="3" :w="2">

下面的函数把累计和与变长数组放在一起，用 `goto` 跳到统一出口：

```c
int sum_positive(int n) {
    int total = 0;
    if (n <= 0) {
        goto out;
    }
    int values[n];
    for (int i = 0; i < n; ++i) {
        values[i] = i;
        total += values[i];
    }
out:
    return total;
}
```

(1) 编译这段代码，贴出错误信息，说明编译器拒绝的是哪一步跳转。

(2) 解释这条限制的理由：如果允许跳入，`values` 的长度由谁决定？

(3) 给出两种改写，都保持 `n <= 0` 时返回 0、`n > 0` 时返回前 `n` 个非负整数之和：

- 把变长数组和循环包进一个内层块，标签留在块外；
- 不再用 `goto`，改为在 `n <= 0` 时直接返回。

分别编译验证，用 `n = 4` 与 `n = 0` 各运行一次，输出应为 `6` 与 `0`。

(4) 另一个人把 `int values[n];` 挪到 `if` 前面，编译通过了。说明为什么编译能过，以及 `n <= 0` 时这个改法为什么仍然不能用。你在自己机器上试运行 `sum_positive(0)` 得到的结果，能作为这个改法正确的证据吗？

</Exercise>

<Exercise id="11421" :d="4" :w="3">

写一个按块复制的文件复制函数，全部失败路径走同一个出口：

```c
int copy_file(const char *src, const char *dst);
```

要求：

1. 以二进制方式打开 `src`，再以二进制写方式创建 `dst`；缓冲区用 `malloc` 申请 4096 字节。
2. 用 `fread` / `fwrite` 按块复制，块长 4096。只有 `fread` 返回小于块长且 `ferror(in)` 为假时才算正常读完。
3. 所有失败路径都 `goto out`，出口按“后取得先释放”的顺序清理。
4. 成功路径上先 `fflush` 输出流、再 `fclose`，两者的返回值都要检查，写入失败要能被发现。出口负责关闭所有仍然打开的流；无论从哪条路径到达出口，同一个流都不得关闭两次。
5. 失败时删除输出文件，但仅当 `dst` 的 `fopen` 已经成功过。源文件打不开时，输出文件不能碰。为此用一个标志记录 `dst` 是否已经打开。
6. 成功返回 0，失败返回 -1。`main` 在失败时向标准错误输出 `copy failed` 并以状态 1 结束；参数个数不对时输出用法并以状态 2 结束。

验收：

- 用 `-std=c17 -Wall -Wextra -Werror` 编译无警告。
- 复制一个文本文件，`cmp` 两个文件无差异。
- 复制一个 10000 字节的二进制文件（大于两个块），`cmp` 无差异。
- `src` 不存在时输出 `copy failed`、状态 1，且不留下空的 `dst`。
- `dst` 指向不存在的目录时同样失败，状态 1。
- 先往 `dst` 写一行旧内容，再用不存在的 `src` 复制：失败后旧内容仍在。

最后回答：第 4 条要求在成功路径上关闭输出流，以便检查 `fclose` 的返回值。如果关闭之后只在第 5 条的标志里记一笔，却不把 `out` 指针本身置空，成功返回走到出口时会发生什么？

</Exercise>
