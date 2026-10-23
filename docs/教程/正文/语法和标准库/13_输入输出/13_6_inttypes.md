# 13.6 `<inttypes.h>`

`<inttypes.h>`（C99 起）包含 `<stdint.h>` 的全部内容，并额外提供用于 `printf`、`scanf` 家族函数的格式化宏。`int32_t`、`uint64_t` 等固定宽度整数类型在不同平台上可能对应不同的基本类型（如 `long` 或 `long long`），所以不能在格式字符串里写死 `%ld` 或 `%lld`；格式化宏会按平台展开为正确的写法。

使用方式：

```c
#include <inttypes.h>
#include <stdio.h>  // 或 <wchar.h> 用于宽字符版本
```

## 1. 格式化宏分类

所有宏均为字符串字面量，扩展为适合平台的格式说明符（如 `"d"`、`"ld"`、`"lld"` 等）。宏名称遵循统一模式：

1. 打印宏（`printf` 系列）以 `PRI` 开头：
   - `PRIdN` / `PRIiN`：有符号十进制（d 或 i）
   - `PRIoN`：无符号八进制（o）
   - `PRIuN`：无符号十进制（u）
   - `PRIxN`：无符号十六进制小写（x）
   - `PRIXN`：无符号十六进制大写（X）

   N 为位宽：8、16、32、64、LEAST8、LEAST16 等，或 `MAX`（对应 `intmax_t`）、`PTR`（对应 `intptr_t`）。

2. 扫描宏（`scanf` 系列）以 `SCN` 开头：
   - `SCNdN` / `SCNiN`：有符号十进制
   - `SCNoN`：无符号八进制
   - `SCNuN`：无符号十进制
   - `SCNxN`：无符号十六进制

   N 同上。

示例宏（实际扩展取决于实现）：

- `PRId32` → `"d"`（32 位系统上可能为 `"ld"`）
- `PRIu64` → `"lu"` 或 `"llu"`
- `PRIdMAX` → `"ld"` 或 `"lld"`（对应 `intmax_t`）
- `SCNxPTR` → `"lx"` 或 `"llx"`

使用时把 `"%"` 与宏相邻书写，例如 `"%" PRId32`（中间没有逗号）。

## 2. 使用示例

```c
#include <inttypes.h>
#include <stdio.h>

int main() {
    int32_t  a = -123456;
    uint64_t b = 18446744073709551615ULL;

    // printf 使用打印宏
    printf("有符号 32 位: %" PRId32 "\n", a);
    printf("无符号 64 位: %" PRIu64 "\n", b);

    // scanf 使用扫描宏
    int64_t c;
    printf("输入有符号 64 位整数：");
    scanf("%" SCNd64, &c);  // 读取有符号64位整数

    return 0;
}
```

可能的输出：

::: terminal title="zsh - test_inttypes"

有符号 32 位: -123456

无符号 64 位: 18446744073709551615

输入有符号 64 位整数：**-2147483648**

:::

## 3. 与宽字符函数配合

格式化宏展开为普通字符串字面量。用于 `fwprintf`、`fwscanf` 等宽字符函数时，可以把它与宽字符串字面量相邻书写，连接后得到宽字符串字面量：

```c
wprintf(L"%" PRId32 L"\n", a);
```

## 4. 其他函数

`<inttypes.h>` 还定义了 `imaxabs`、`imaxdiv` 函数（用于 `intmax_t` 的绝对值和除法，`imaxdiv` 的结果类型为 `imaxdiv_t`）。

## 5. 习题

### 5.1 巩固

<Exercise id="11316" :d="1" :w="2" multi>

下面的程序在 64 位 Windows（LLP64，`long` 为 32 位）与 64 位 Linux（LP64，`long` 为 64 位）上表现不同：

```c
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    int32_t level = -5;
    uint32_t mask = 0xFFFF0000u;
    int64_t delta = -5000000000LL;
    uint64_t total = 1234567890123ULL;

    printf("[%5d] [%08x]\n", level, mask);
    printf("[%ld] [%lu]\n", delta, total);
    return 0;
}
```

(1) _[1.2]_ 建一个空文件 `empty.c`，运行 `gcc -dM -E empty.c`，查出 `__SIZEOF_LONG__` 与 `__SIZEOF_LONG_LONG__`；再用 `gcc -std=c17 -Wall -Wextra` 编译上面的程序。写出本机报出的警告，并解释为什么在 LLP64 上 `int64_t` 与 `long` 不是同一个类型，而在 LP64 上编译不报警。在 64 位 Windows 上第二行输出 `[-705032704] [1912276171]`，LP64 平台上同一份代码输出的却是正确值。从实参的传递方式解释低 32 位从哪里来，并说明这种写法为什么属于未定义行为。

(2) _[2.2]_ 把 `#include <stdint.h>` 换成 `#include <inttypes.h>`（它包含 `<stdint.h>`），再改写第二行 `printf` 的格式串，让两种平台都输出正确值且 `-Wall -Wextra` 零警告。期望输出为 `[   -5] [ffff0000]` 与 `[-5000000000] [1234567890123]`。第一行为什么不用改？

(3) _[2.1]_ 下面两行各写错了一处（`delta` 是 `int64_t`，值为 -9000000000），分别补进程序编译运行，写出输出和警告：

```c
printf(PRId64 "\n", delta);   /* 错误一 */
printf("%PRId64\n", delta);   /* 错误二 */
```

第一行在 64 位 Windows 上输出 `lld`、在 LP64 上输出 `ld`，第二行在 MinGW 上输出 `PRId64`、在 glibc 上输出 `%PRId64`。对照 `gcc -Wall -Wextra` 的警告：第一行一条不报，第二行报 `unknown conversion type character 'P'` 与 `too many arguments for format`。说明两处错法的区别：第一行里 `delta` 去了哪里，格式检查为什么没有看出来；第二行又是从哪条线索被发现的。再写出两行的正确写法，并说明为什么 GCC 对第一行这种错法无能为力。

(4) _[3.1]_ 运行 `gcc -dM -E -include inttypes.h empty.c`，找出本机 `PRId8`、`PRId64`、`PRIuLEAST16`、`SCNd8`、`PRIxPTR` 的展开值。`PRId8` 展开成 `"d"`，而同样对应 8 位有符号类型的 `SCNd8` 展开成 `"hhd"`，说明这两个长度修饰符分别是为了配合什么。宏清单里有的条目并不直接是字符串字面量：MinGW 上 `PRIxPTR` 展开成 `PRIx64`，glibc 上 `PRId64` 展开成 `__PRI64_PREFIX "d"`。解释这种层层定义的好处，以及为什么这不妨碍 `"%" PRId64` 直接拼出可用的格式串。

(5) _[3.3]_ 下面这段用打印宏读入：

```c
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    int8_t level = 0;
    if (scanf("%" PRId8, &level) != 1) {
        return 1;
    }
    printf("%" PRId8 "\n", level);
    return 0;
}
```

编译并记下警告，然后分别输入 `100` 与 `1000` 运行。把 `level` 换成 `int8_t cell[4] = { 0, 0x7F, 0x11, 0x22 };`，用同一个格式串读入 `cell[0]`，逐个转换成 `unsigned char` 后按 `%02x` 打印四个字节，两次输入得到的字节序列分别是 `64 00 00 00` 与 `e8 03 00 00`。解释 `1000` 为什么让 `cell[0]` 变成 -24、后三个字节为什么被清零，再改用 `SCNd8` 重跑，确认只剩 `cell[0]` 被写、`7f 11 22` 原样保留。最后说明这一段里两种宏各管读还是写。MinGW 默认的 `scanf` 实现在 `-Wformat` 检查里不认 `hh`，所以改用 `SCNd8` 后仍可能报 `unknown conversion type character 'h'`；那是格式检查的误报，运行结果正确，加 `-D__USE_MINGW_ANSI_STDIO=1` 编译可以让警告消失。

</Exercise>

### 5.2 编程

<Exercise id="11317" :d="3" :w="3">

写一个程序 `dump_records.c`：从标准输入读入若干行「地址 值」，每行是一个十六进制地址和一个十进制有符号 64 位整数，输出对齐的表格。输入文件 `records.txt`：

```text
0x1000 -42
FFFF 9223372036854775807
7f 42
```

要求：

1. 用 `SCNxPTR` 读地址、用 `SCNd64` 读值；`scanf` 的返回值不为 2 时立即停止，不再处理这一行之后的内容。
2. 地址补零到 `2 * sizeof(uintptr_t)` 位十六进制，宽度用 `*` 作为实参传入，不要写死 16。`uintptr_t` 是 `<stdint.h>` 中能容纳指针值的无符号整数类型，与 `PRIxPTR` 配对。
3. 值右对齐，宽度 20。
4. 每行前面输出从 1 开始的行号，全部结束后输出处理的行数。

期望输出：

```out
  1  0x0000000000001000                   -42
  2  0x000000000000ffff   9223372036854775807
  3  0x000000000000007f                    42
3 行
```

验收：

1. 用上面的 `records.txt` 运行，逐字节比对期望输出。
2. 在文件末尾追加一行 `zzz 1`，程序应只输出上面三行和 `3 行`，不能卡住或重复计数。
3. 查 32 位目标下 `PRIxPTR` 的展开值：MinGW-w64 可以直接运行 `gcc -m32 -dM -E -include inttypes.h -`；缺少 32 位头文件时，打开工具链的 `inttypes.h`，找到指针宽度为 4 的那个分支。说明 32 位目标上地址列为什么应该是 8 位十六进制，以及这里为什么不能改用 `PRIx64` 打印地址。

</Exercise>

<Exercise id="11318" :d="4" :w="3">

实现一个与 `snprintf` 用法相同的进制转换函数，只处理 `intmax_t`：

```c
#include <stddef.h>
#include <stdint.h>

/* 把 v 按 base 进制写入 buf，最多写入 n - 1 个字符再加终止符。
 * 返回完整表示所需的字符数（不含终止符）；n 为 0 时不写入 buf。
 * base 不在 2～36 时返回 0。 */
size_t fmt_imax(intmax_t v, unsigned base, char *buf, size_t n);
```

要求：

1. 数字用小写字母，负数的表示前面加 `-`，`0` 的表示是 `"0"`。
2. 商和余数用本节的 `imaxdiv` 取得，不要直接写 `/` 和 `%`；也不得调用 `snprintf`、`sprintf` 完成转换。
3. `INTMAX_MIN` 必须得到正确结果，不能靠 `-v` 取绝对值。`imaxabs(INTMAX_MIN)` 同样没有定义（结果不是可表示的正数），不能借用它绕过这个问题。
4. 截断时按 `snprintf` 的约定写入前 `n - 1` 个字符并补终止符，返回值仍是完整长度。
5. 用 `PRIdMAX` 打印输入与结果，用 `SCNiMAX` 把 `INTMAX_MIN` 的十进制表示读回来，确认与原值相等。再打印 `imaxdiv(-17, 5)` 的商与余数，用 `imaxabs` 取余数的绝对值，结果应为 `-3`、`-2` 和 `2`。

验收（`buf` 为 80 字节时）：

| 调用 | 返回值 | `buf` |
| --- | --- | --- |
| `fmt_imax(0, 16, buf, 80)` | 1 | `0` |
| `fmt_imax(-255, 16, buf, 80)` | 3 | `-ff` |
| `fmt_imax(255, 36, buf, 80)` | 2 | `73` |
| `fmt_imax(INTMAX_MAX, 16, buf, 80)` | 16 | `7fffffffffffffff` |
| `fmt_imax(INTMAX_MIN, 16, buf, 80)` | 17 | `-8000000000000000` |

另外验证：`fmt_imax(INTMAX_MIN, 2, small, 8)` 返回 65，`small` 的内容是 `-100000`；`fmt_imax(-255, 16, NULL, 0)` 返回 3。

提示：`imaxdiv` 的余数符号跟随被除数，取位时要处理符号；循环到商为 0 为止，个位从缓冲区尾部向前填。

</Exercise>

### 5.3 综合与挑战

<Exercise id="11319" :d="2" :w="2" multi>

(1) _[2.2]_ 下面这段程序想把一个 `int64_t` 写到宽字符流上：

```c
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

int main(void)
{
    int64_t offset = -9000000000LL;
    int n = fwprintf(stdout, "%" PRId64 "\n", offset);
    wprintf(L"n = %d\n", n);
    return 0;
}
```

用 GCC 14 在 Linux 上编译，会报 `passing argument 2 of 'fwprintf' from incompatible pointer type` 并停止；用 MinGW GCC 13 在 Windows 上只有同样的警告，程序能运行，却只输出 `n = -1`。解释这两种结果的来由，然后只改格式串这一处让它输出 `-9000000000`，并说明为什么补一个 `L` 就够。

(2) _[4.3]_ 把三条记录写成宽字符行（`INT64_MIN` 即 -9223372036854775808）：

| 名称 | `int64_t` 值 |
| --- | --- |
| `count` | `1234567890123` |
| `offset` | `-1` |
| `minimum` | `INT64_MIN` |

每行的格式是：名称左对齐宽度 8，空一格后是右对齐宽度 20 的十进制，然后三个空格，再是 `0x` 加 16 位十六进制。用 `swprintf` 配合 `PRId64`、`PRIx64` 完成，名称用 `%ls` 输出 `wchar_t` 字符串。`swprintf` 是 `snprintf` 的宽字符版本：

```c
int swprintf(wchar_t *s, size_t n, const wchar_t *format, ...);
```

期望输出：

```out
count           1234567890123   0x0000011f71fb04cb
offset                     -1   0xffffffffffffffff
minimum  -9223372036854775808   0x8000000000000000
```

先用 32 个 `wchar_t` 的缓冲区，把格式化结果写进去，再用 `fwprintf(stdout, L"%ls\n", buf)` 输出；记下每次 `swprintf` 的返回值。再把容量翻倍到 64 重跑。三行的 `swprintf` 返回值在两种容量下各是多少？说明 `swprintf` 放不下时的返回值与 `snprintf` 有什么不同，为什么这里不能像 `snprintf` 那样靠返回值得到所需容量。最后把重试写成循环：容量从 8 开始翻倍，成功或达到 256 为止。

(3) _[5.4*]_ 写一个宏 `WPRINT(x)`，用 `_Generic` 按实参类型选择 `L"%" PRId8`、`L"%" PRId64`、`L"%" PRIu64` 一类的格式串，交给 `fwprintf` 输出。至少覆盖 `int8_t`、`uint16_t`、`int64_t`、`uint64_t` 四种类型，实参依次为 -7、60000、1234567890123、`UINT64_MAX`，输出一行：

```out
-7 60000 1234567890123 18446744073709551615
```

说明 `_Generic` 的关联列表里为什么不能同时写 `int32_t` 和 `int`，GCC 在这种情况下给出什么错误。

</Exercise>
