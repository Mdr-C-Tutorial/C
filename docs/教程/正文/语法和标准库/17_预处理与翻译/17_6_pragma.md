# 17.6 `#pragma`

这个预处理指令可以控制一些**实现定义的行为**。

## 1. 标准行为

在下面这三种用法中，_`arg`_ 是 `ON` 和 `OFF` 和 `DEFAULT` 三者之一。

### 1.1 `#pragma STDC FENV_ACCESS` _`arg`_

如果这个 pragma 被设置为 `ON`，会告知编译器：这个程序会访问或者修改[**浮点环境**](/教程/正文/语法和标准库/19_数值_位运算与随机数/19_10_fenv)。这可能导致某些和浮点数相关的编译器优化被禁用。

默认值由实现定义，一般为 `OFF`。

### 1.2 `#pragma STDC FP_CONTRACT` _`arg`_

这个指令控制是否允许把多个浮点运算收缩 (contract) 成单条复合指令，例如把 `a * b + c` 收缩为融合乘加。开启后可能得到更高性能，也可能改变最后一位舍入结果。

```c
#pragma STDC FP_CONTRACT ON
```

如果你的数值算法要求严格可复现结果，应结合编译器文档确认默认值并显式设置。

### 1.3 `#pragma STDC CX_LIMITED_RANGE` _`arg`_

这个指令用于复数运算的范围语义。设置为 `ON` 时，实现可在更宽松的中间范围假设下优化复数表达式；设置为 `OFF` 时，优先保持更严格的数值语义。它主要影响 `<complex.h>` 相关计算。

这两个 pragma 都是“给编译器的语义提示”，不是强制“必须生成某条特定机器指令”。

## 2. 非标准行为

下面的 `#pragma` 用法不是 ISO C 规定的，但受到了许多实现支持。

### 2.1 `#pragma pack`

这个指令控制后续定义的结构体和联合体成员的最大对齐。

典型写法如下：

```c
#pragma pack(push, 1)
struct hdr {
    unsigned short type;
    unsigned int len;
};
#pragma pack(pop)
```

对齐调整会影响布局、访问性能，甚至在某些平台导致未对齐访问异常。除非必须与外部二进制格式对齐，否则不要随意使用。

## 3. `_Pragma()`

`_Pragma` 是一个运算符，可在宏展开中生成 pragma，等价于“可编程的 `#pragma`”。它最常见的用途是把编译器诊断控制封装成宏。

```c
#define DO_PRAGMA(x) _Pragma(#x)
#define DISABLE_UNUSED_WARNING DO_PRAGMA(GCC diagnostic ignored "-Wunused-parameter")
```

当需要在宏内部携带 pragma 时，应使用 `_Pragma`，而不是直接写 `#pragma`。

## 4. 习题

<Exercise id="11722" :d="2" :w="3">

下面五个结构体的字段完全相同，只有外面的 `#pragma pack` 不同：

```c
#include <stddef.h>
#include <stdio.h>

struct s0 { char c; int i; short s; };
#pragma pack(push, 1)
struct s1 { char c; int i; short s; };
#pragma pack(push, 2)
struct s2 { char c; int i; short s; };
#pragma pack(pop)
struct s3 { char c; int i; short s; };
#pragma pack(pop)
struct s4 { char c; int i; short s; };

int main(void) {
    printf("%d/%d %d/%d %d/%d %d/%d %d/%d\n",
           (int)sizeof(struct s0), (int)_Alignof(struct s0),
           (int)sizeof(struct s1), (int)_Alignof(struct s1),
           (int)sizeof(struct s2), (int)_Alignof(struct s2),
           (int)sizeof(struct s3), (int)_Alignof(struct s3),
           (int)sizeof(struct s4), (int)_Alignof(struct s4));
    printf("%d %d %d %d %d\n",
           (int)offsetof(struct s0, i), (int)offsetof(struct s1, i), (int)offsetof(struct s2, i),
           (int)offsetof(struct s3, i), (int)offsetof(struct s4, i));
    printf("%d %d %d %d %d\n",
           (int)offsetof(struct s0, s), (int)offsetof(struct s1, s), (int)offsetof(struct s2, s),
           (int)offsetof(struct s3, s), (int)offsetof(struct s4, s));
    return 0;
}
```

1. 写出三行输出。`_Alignof(struct s1)` 是 1 还是 4？
2. `s2` 在两个 `push` 之间，`s3` 在内层 `pop` 之后，`s4` 在外层 `pop` 之后。写出三者的大小。内层 `pop` 恢复的是 8（默认值）还是 1（外层 `push` 的实参）？这决定了 `s3` 按哪个上限对齐。
3. 删掉 `s3` 之前的那一行 `#pragma pack(pop)`，重新运行。`s3`、`s4` 的大小各变成多少？这说明 `pack` 的状态会一直延续到下一次 `push`/`pop`，对这段区间里的每个定义都生效，不限于紧随其后的那一个结构体。
4. `pack` 只是把成员的对齐上限压低，不改变成员的类型与 `_Alignof(char)`、`_Alignof(short)` 这些类型的本征对齐。用 `_Alignof(double)` 与 `#pragma pack(push, 4)` 下的 `struct { char c; double d; }` 验证这句话：结构体的对齐是 4，`d` 的偏移是 4，而 `double` 本身仍然要求 8。

</Exercise>

<Exercise id="11723" :d="3" :w="3">

正文给出了 `DO_PRAGMA` 与 `DISABLE_UNUSED_WARNING` 两个宏。下面把这个思路做成一对可嵌套的开关：

```c
#define MDR_PRAGMA(x) _Pragma(#x)
#define MDR_DIAG_PUSH MDR_PRAGMA(GCC diagnostic push)
#define MDR_DIAG_POP MDR_PRAGMA(GCC diagnostic pop)
#define MDR_DIAG_IGNORE(w) MDR_PRAGMA(GCC diagnostic ignored w)
```

1. 用 `MDR_DIAG_PUSH`、`MDR_DIAG_IGNORE("-Wunused-parameter")`、`MDR_DIAG_POP` 包住一个带未用参数的函数，另一个同样带未用参数的函数放在 `POP` 之后。用 `gcc -std=c17 -Wall -Wextra` 编译，写出哪个函数报、哪个不报。
2. 把 `MDR_DIAG_POP` 删掉，再编译一次。第二个函数还报不报？这说明 `diagnostic push/pop` 的作用域是从哪里到哪里。
3. 把 `MDR_DIAG_IGNORE` 的参数直接写成 `MDR_PRAGMA(GCC diagnostic ignored WARN)`，其中 `#define WARN "-Wunused-parameter"`。记录报错或警告，说明 `#` 把什么变成了字符串、`_Pragma` 又对圆括号里的记号做了什么。
4. 直接写 `#define PACK_BEGIN #pragma pack(push, 1)` 是不行的。写下报错原文，并用标准里「预处理指令不能出现在宏的替换列表里」这条规则解释；再用 `_Pragma` 把它改对，用 `sizeof` 验证生效。
5. `_Pragma` 的圆括号里只能是一个字符串字面量。试 `_Pragma(GCC diagnostic push)`、`_Pragma("GCC diagnostic " "push")`、`_Pragma(W)`（`W` 是宏，值为一个字符串）三种写法，记录编译器各自的反应，并说明为什么 `DO_PRAGMA` 要写成 `_Pragma(#x)`。

</Exercise>

<Exercise id="11724" :d="4" :w="3">

某二进制格式的记录头是 `tag` 2 字节、`len` 4 字节、`flags` 2 字节、`crc` 4 字节，按小端存放，共 12 字节。下面两个程序分别按「加 `pack`」与「不加 `pack`」打印布局：

```c
#include <stddef.h>
#include <stdio.h>

#pragma pack(push, 1)
struct rec {
    unsigned short type;
    unsigned int len;
    unsigned short flags;
    unsigned int crc;
};
#pragma pack(pop)

int main(void) {
    printf("%d %d %d %d %d\n", (int)sizeof(struct rec),
           (int)offsetof(struct rec, type), (int)offsetof(struct rec, len),
           (int)offsetof(struct rec, flags), (int)offsetof(struct rec, crc));
    return 0;
}
```

1. 写出加了 `pack` 时的输出；删掉两行 `#pragma` 再运行一次，写出不加时的输出，并逐字段算出这四个偏移。
2. 用这个结构体 `fwrite` 一条记录（`type = 0x1234`、`len = 100`、`flags = 1`、`crc = 0xDEADBEEF`），用 `xxd` 或 `od -An -tx1` 看文件的前 12 个字节。按字段把字节切开，说明每个字段内部是什么顺序；如果换成大端机器，这 12 个字节会不会变。
3. 读文件的翻译单元漏写了 `#pragma pack`，写文件的翻译单元写了。把文件里那 12 个字节读进 `unsigned char buf[16] = { 0 };`（多出的 4 个字节留作零），再用 `memcpy` 分别搬进带 `pack` 与不带 `pack` 的同字段结构体，打印两组的四个字段。哪几个字段读对了、哪几个读错了？错掉的值是从哪几个字节拼出来的？
4. 把 `#pragma pack(push, 1)` 换成 `pack(push, 2)`，`len` 与 `crc` 的偏移变了吗？为什么？再换成 `pack(push, 4)`，这次的偏移与 `sizeof` 是多少？三个取值放在一起，说明 `pack` 的实参是「对齐上限」而不是「成员必须按这个值对齐」。
5. 如果这段结构体定义放在头文件里，`#pragma pack(pop)` 写在 `#endif` 之后会有什么后果？同一个翻译单元里第二次包含这个头文件时，包含保护会跳过 `push`，而 `pop` 照样执行。写一个实验：在一个 `#pragma pack(push, 4)` 块里再次包含该头文件，打印外层结构体的大小与对齐，再把它改成写在 `#endif` 里面重做一次。最后用 `_Static_assert` 把「结构体的对齐被包含的头文件改掉」变成编译错误，防止以后有人又把它挪出去。
6. `pack` 之后，`&r.len` 这个地址还能当成 `unsigned int *` 用吗？把 `main` 里的对象改成 `struct rec r[2];`，打印 `(uintptr_t)&r[0].len % _Alignof(unsigned int)` 与 `_Alignof(unsigned int)` 两个值，说明成员地址违反了什么要求。再写一个函数 `unsigned int get_len(const struct rec *r) { return *(const unsigned int *)&r->len; }`，用它读数据，并在报告里说明：这条语句在要求严格对齐的架构上会怎样，以及为什么正文推荐「只在与外部格式对齐时才用 `pack`」。

</Exercise>

<Exercise id="11725" :d="5" :w="4">

`#pragma STDC FP_CONTRACT` 说「允许把 `a * b + c` 收缩成一条融合乘加」。这句话的实际效果取决于编译器与命令行选项，这一题用实验把它量出来。

1. 写一个 `f(double a, double b) { return a * b - 1.0; }`，主程序取 `a = 1.0 + 0x1p-27`、`b = 1.0 - 0x1p-27`（两者都是 `volatile`，防止常量折叠），用 `printf("%a")` 打印 `f(a, b)`。分别用 GCC 与 Clang 各编译运行一次，记录结果。再换成 `-std=gnu17` 编译一次。三次结果是否相同？`-std=c17` 与 `-std=gnu17` 的差别说明 GCC 的默认值是什么。
2. 在 `f` 前面加 `#pragma STDC FP_CONTRACT OFF`，把第 1 步的几条命令各再跑一遍。结果有变化吗？GCC 13.2 加 `-Wall -Wextra` 时对这个 pragma 报什么？Clang 19 呢？这决定了后面读结果时该怎么理解 GCC 的输出。
3. 用 `-S` 生成汇编，看 `f` 里乘法与减法是否被合并成一条融合乘加指令（x86-64 上需要打开 `-mfma` 才会出现，GCC 与 Clang 各试一次）。把第 1、2 步的结果与这条指令出现与否对应起来：pragma 和命令行分别起了什么作用。
4. 用 `fma` 函数（`<math.h>`）算出同一次运算的「真」融合结果，与第 1 步的分离结果比较。两个值差多少？这个差从哪来？
5. `#pragma STDC FENV_ACCESS ON` 的效果同理：没有它，编译器可以假设计算不关心舍入模式，把常量除法在编译期算掉。写一个 `fesetround(FE_UPWARD)` 后计算 `double third = 1.0 / 3.0;` 的程序，用 `printf("%a")` 打印，用 Clang 运行；再在 `main` 的花括号之后、第一条语句之前加 `#pragma STDC FENV_ACCESS ON` 跑一次。`FE_UPWARD` 的结果应当是 `0x1.5555555555556p-2`，哪几次输出的不是它？GCC 加同样的 pragma 又输出什么？

</Exercise>
