# 6.2 return

`return` 用于结束当前函数执行，并把控制权交还调用者。

## 1. 返回值规则

非 `void` 函数应返回与声明兼容的表达式；`void` 函数可写 `return;` 提前结束。把 `return` 放在分支入口处（卫语句）通常能减少嵌套深度，让主流程更平直。

```c
#include <stdbool.h>

bool is_valid_score(int score) {
    if (score < 0 || score > 100) {
        return false;
    }
    return true;
}
```

## 2. 与对象生存期的关系

`return` 执行时，当前函数的自动存储期对象会结束生命周期。因此不要返回指向局部对象的指针。

```c
int *bad(void) {
    int x = 42;
    return &x; /* 错误：返回悬垂指针 */
}
```

可以改为直接返回值本身、由调用者提供存储，或返回动态分配的存储（见第 11 章）并约定由调用者释放。

## 3. `main` 的返回语义

在宿主环境中，`main` 返回值会作为进程退出状态交给执行环境。通常约定 `0`（或 `EXIT_SUCCESS`）表示成功，非零（或 `EXIT_FAILURE`）表示失败。把失败原因映射到稳定返回码，能让脚本、CI 和调用链更容易判断执行结果。

```c
#include <stdlib.h>

int main(void) {
    int ok = 1;
    if (!ok) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

## 4. 非 `void` 函数的完整返回

::: concept function-end-without-return title="执行到非 void 函数末尾"

如果非 `void` 函数执行到末尾的 `}` 而没有执行 `return`，并且调用者使用了这次调用的值，行为未定义。`main` 是例外：执行到 `main` 末尾的 `}` 相当于返回 `0`。

:::

写分支时应确保每条路径都有明确返回，避免把“理论不会走到这里”当作隐式前提。

## 5. 返回结构体

函数可以返回结构体对象（结构体在第 10 章介绍），一次返回多个相关的值，从而减少用于输出的指针形参。需要同时报告成功与否时，可以在返回的结构体中加入表示状态的成员，或用返回值表示状态、用指针形参输出结果。

## 6. 习题

### 6.1 巩固

<Exercise id="10610" :d="2" :w="3">

下面的程序里，`grade` 有一条路径走不到 `return`，`report` 用 `return;` 提前结束。

```c
#include <stdio.h>

int clamp(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

int grade(int score) {
    if (score >= 85) {
        return 4;
    } else if (score >= 70) {
        return 3;
    } else if (score >= 60) {
        return 2;
    }
}

void report(int score) {
    if (score < 0 || score > 100) {
        printf("invalid\n");
        return;
    }
    printf("grade %d\n", grade(score));
}

int main(void) {
    printf("%d %d\n", clamp(-5), clamp(150));
    printf("%d\n", grade(50));
    report(85);
    return 0;
}
```

1. 用 `gcc -std=c17 -Wall -Wextra` 构建，抄下警告，指出它对应哪个函数、哪一行。编译能通过吗？
2. 运行并记录三行输出。第二行打印的值可以当作 `grade(50)` 的值吗？为什么？
3. 补上缺的返回语句，使 `grade(50)` 为 `1`，重新构建。`-Wall -Wextra` 必须无输出，三行输出变成 `0 100`、`1`、`grade 4`。
4. 把 `report` 里的 `return;` 改成 `break;`，构建并抄下 error。说明 `return` 与 `break` 各自作用在什么范围内。
5. 删掉 `main` 末尾的 `return 0;`，重新构建运行。用 `echo $?`（bash）或 `$LASTEXITCODE`（PowerShell）查看退出状态，说明执行到 `main` 的 `}` 相当于什么。

</Exercise>

<Exercise id="10611" :d="2" :w="3">

`main` 的返回值就是进程的退出状态，调用它的脚本靠这个数字判断成败。

1. 写一个程序：从标准输入读一个分数，60 及以上返回 `0`，60 以下返回 `1`，分数不在 0～100 或读入失败返回 `2`。用 `85`、`42`、`-5`、`120`、`abc` 五次输入运行，记录每次的退出状态。
2. 把成功分支改成 `return 256;`，失败分支改成 `return 300;`，其余不动。两种 shell 各看到什么数字？再用 `return -1;` 试一次。
3. 根据记录的数字说明宿主环境保留了多少位退出状态。这个位数由 C 标准规定，还是由本机的执行环境决定？只返回 `0` 和 `1` 的程序会不会受这个差别影响？
4. 把返回值改成 `EXIT_SUCCESS` 与 `EXIT_FAILURE`。为什么脚本和 CI 应当只依赖这两个宏，而不依赖具体数值？

验收：第 1 步五个输入依次得到 `0`、`1`、`2`、`2`、`2`。第 2 步 bash 看到的 `256` 变成 `0`，`300` 变成 `44`，PowerShell 保留原值。

</Exercise>

### 6.2 编程

<Exercise id="10612" :d="3" :w="3">

下面的函数判断三角形种类：`3` 为等边，`2` 为等腰，`1` 为普通三角形，`0` 为不能构成三角形。`classify` 有嵌套，`report` 已经用了卫语句。

```c
#include <stdio.h>

int classify(int a, int b, int c) {
    if (a + b > c && a + c > b && b + c > a) {
        if (a == b) {
            if (b == c) {
                return 3;
            } else {
                return 2;
            }
        } else {
            if (b == c || a == c) {
                return 2;
            } else {
                return 1;
            }
        }
    } else {
        return 0;
    }
}

void report(int a, int b, int c) {
    if (a <= 0 || b <= 0 || c <= 0) {
        printf("bad: %d %d %d\n", a, b, c);
        return;
    }
    printf("kind = %d\n", classify(a, b, c));
}

int main(void) {
    printf("%d %d %d %d\n", classify(3, 3, 3), classify(3, 3, 4), classify(3, 4, 5), classify(1, 2, 3));
    report(3, 4, 5);
    report(-1, 2, 3);
    return 0;
}
```

1. 用卫语句改写 `classify`：不能构成三角形时立刻返回 `0`，再依次判断等边、等腰，最后返回 `1`。要求最多四条 `return`，且没有嵌套的 `if/else`。
2. 保留原版本，改名 `classify_nested`。写一个三重循环，对 `1`～`8` 的所有三元组比较两个版本，统计不一致的次数，正确结果是 `0`。
3. 改写前后各运行一次，`main` 的输出必须完全相同。
4. 数一数两个版本从函数入口到每条 `return` 之间的最大缩进层数。如果 `report` 不用卫语句，`printf("kind = %d\n", ...)` 要缩进几层？说明卫语句省掉的是什么。

验收：第 3 步两行输出为 `3 2 1 0` 与 `kind = 1`，另有 `bad: -1 2 3`。

</Exercise>

<Exercise id="10613" :d="3" :w="3">

四个函数都只返回一个值，其中两个返回的地址在函数返回后失效。

```c
#include <stdio.h>

int *first(void) {
    int value = 42;
    return &value;
}

char *second(void) {
    char buf[] = "hi";
    return buf;
}

const char *third(void) {
    return "hi";
}

int fourth(void) {
    int value = 42;
    return value;
}
```

1. 逐个判断返回值在函数返回后是否仍然有效，写出理由。编译时抄下 `-Wreturn-local-addr` 的警告，指出它对应哪两个函数。
2. 给 `first` 与 `second` 各写一个安全版本，要求调用方仍能拿到可用数据，且不要求调用者释放存储。`first` 可以改用静态存储期的对象；`second` 可以返回字符串字面量，也可以让调用方传入缓冲区，两种都写一遍。
3. 用下面的测试验证静态存储期版本：

   ```c
   int *p = first();
   int *q = first();
   printf("%d %d %d\n", p == q, *p, *q);
   ```

   让静态对象每次调用先加一，两次读到的值就是 `2` 和 `2`。连续两次调用拿到的是不是同一个对象？如果两个调用方都以为拿到各自的副本，会出什么问题？
4. 为什么 `third` 与 `fourth` 没有警告？把 `third` 的返回类型从 `const char *` 改成 `char *`，只加 `-Wall -Wextra` 编译一次，再加上 `-Wwrite-strings` 编译一次，比较两次的诊断。然后照这个 `char *` 版本写 `p[0] = 'H';` 改写字符串字面量，运行并记录结果。

验收：第 3 步输出 `1 2 2`；第 4 步只有加了 `-Wwrite-strings` 才报 `return discards 'const' qualifier`，而写入那一行在本机是段错误。

</Exercise>

### 6.3 综合与挑战

<Exercise id="10614" :d="4" :w="4" scope="**">

同一件事有两种接口：返回结构体，或者返回状态、用指针形参输出结果。结构体在第 10 章才正式讲，这里只按下面的定义使用。

```c
#include <stdbool.h>
#include <stdio.h>

struct div_result {
    int quotient;
    int remainder;
    int status; /* 0 表示成功，1 表示除数为 0 */
};

struct div_result divide(int dividend, int divisor);

bool divide_out(int dividend, int divisor, int *quotient, int *remainder);
```

1. 实现 `divide`：除数为 0 时返回 `status` 为 `1`、其余成员为 `0` 的结构体；否则填好商与余数，`status` 为 `0`。
2. 实现 `divide_out`：除数为 0 时返回 `false`，且不修改两个输出对象；成功时写入结果并返回 `true`。
3. 对 `17 / 5` 与 `17 / 0` 各调用两个函数，输出全部结果。验收为四行 `3 2 0`、`0 0 1`、`1 3 2`、`0 0 0`。
4. 第 2 问里「失败时不修改输出对象」要靠函数体里的分支保证。调用方如果先给 `q`、`r` 赋初值再调用，失败后能读到什么？编译器能不能检查出某次实现违反了这条约定？
5. 比较两种写法在调用处的形式：哪个能直接写在 `printf` 的实参里？一次要返回五个值时，两种写法各要写多少行？需要同时报告成败的场合，你选哪一种，理由是什么。

</Exercise>
