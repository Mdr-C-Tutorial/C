# 19.1 `<stdint.h>`

`<stdint.h>` 由 C99 引入，定义一组指定宽度的整数类型，以及描述这些类型取值范围的宏。`int`、`long` 等标准整数类型的宽度由实现决定；需要确切宽度或最小宽度时，应使用本头文件中的类型。

```c
#include <stdint.h>
```

## 1. 精确宽度整数类型（Exact-width integer types）

这些类型表示精确位宽的有符号和无符号整数。如果实现不支持对应宽度，则不定义该类型。

- 有符号：`int8_t`、`int16_t`、`int32_t`、`int64_t`
- 无符号：`uint8_t`、`uint16_t`、`uint32_t`、`uint64_t`

```c
int32_t x = -1000000;   // 精确 32 位有符号整数
uint8_t y = 255;        // 精确 8 位无符号整数，范围 0~255
```

## 2. 最小宽度整数类型（Least-width integer types）

这些类型至少具有指定位宽，实现可提供更大宽度。

- 有符号：`int_least8_t`、`int_least16_t`、`int_least32_t`、`int_least64_t`
- 无符号：`uint_least8_t`、`uint_least16_t`、`uint_least32_t`、`uint_least64_t`

## 3. 最快最小宽度整数类型（Fastest minimum-width integer types）

这些类型至少具有指定位宽，实现选择运算速度最快的类型（可能更大）。

- 有符号：`int_fast8_t`、`int_fast16_t`、`int_fast32_t`、`int_fast64_t`
- 无符号：`uint_fast8_t`、`uint_fast16_t`、`uint_fast32_t`、`uint_fast64_t`

## 4. 指针整数类型（Integer types capable of holding object pointers）

- `intptr_t`：有符号整数类型，可容纳转换后的指针值（可选）。
- `uintptr_t`：无符号整数类型，可容纳转换后的指针值（可选）。

```c
int x = 0;
void *ptr = &x;
uintptr_t addr = (uintptr_t)ptr;  // 将指针转换为整数
```

## 5. 最大宽度整数类型（Greatest-width integer types）

- `intmax_t`：有符号整数类型，至少与任何其他有符号整数类型一样宽。
- `uintmax_t`：无符号整数类型，至少与任何其他无符号整数类型一样宽。

## 6. 极限宏（Limits macros）

为上述所有类型定义最小值和最大值宏，例如：

- `INT8_MIN`、`INT8_MAX`、`UINT8_MAX`
- `INT32_MIN`、`INT32_MAX`、`UINT32_MAX`
- 同理适用于 16/32/64 位以及 least/fast/max 类型。

`<limits.h>` 给出的是 `int`、`long` 等标准整数类型的极限（见 19.2），二者的宏名并不重叠。

## 7. 其他宏

- `PTRDIFF_MIN`、`PTRDIFF_MAX`：`ptrdiff_t` 的极限。
- `SIZE_MAX`：`size_t` 的最大值。
- `SIG_ATOMIC_MIN`、`SIG_ATOMIC_MAX`：`sig_atomic_t` 的极限。
- `WCHAR_MIN`、`WCHAR_MAX`：`wchar_t` 的极限。
- `WINT_MIN`、`WINT_MAX`：`wint_t` 的极限。

## 8. 格式化输出

这些类型的格式化输出需配合 `<inttypes.h>` 中的宏（如 `PRId32` 用于 `printf("%" PRId32, x);`）。

## 9. 习题

### 9.1 巩固

<Exercise id="11918" :d="1" :w="2" multi>

`<stdint.h>` 的三类宽度类型在同一平台上未必等宽。运行下面的程序：

```c
#include <stdint.h>
#include <stdio.h>

int main(void) {
    printf("%d %d %d %d\n", (int)sizeof(int8_t), (int)sizeof(int_least8_t),
           (int)sizeof(int_fast8_t), (int)sizeof(int_fast16_t));
    printf("%d\n", (int)INT_FAST8_MAX);
    return 0;
}
```

(1) _[0.1]_ 抄下输出。本机 MinGW-w64 上是 `1 1 1 2` 与 `127`。

(2) _[2.2]_ 把 `int_fast8_t` 换成 `int_fast64_t`，把 `INT_FAST8_MAX` 换成 `INT_FAST64_MAX`，用 `(long long)` 转换后配 `%lld` 打印，预测再运行。本机结果是 `8 9223372036854775807`。为什么 `int_fast16_t` 与 `int16_t` 等宽，而 `int_fast8_t` 不必与 `int8_t` 等宽？

(3) _[3.3]_ 用 `arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -dM -E -include stdint.h - < /dev/null` 查 Cortex-M3 上 `INT_FAST8_MAX` 与 `INT_FAST16_MAX` 的值，与主机对比。Cortex-M3 上这两个宏展开成多少？为什么与主机上的 `127`、`32767` 不同？

(4) _[2.2]_ 把主机上的 `sizeof` 结果写成 `_Static_assert`（C11），例如 `_Static_assert(sizeof(int_fast8_t) == 1, "fast8");`，在主机与 Cortex-M3 上各编译一次。记录交叉目标上失败的是哪几条。

</Exercise>

<Exercise id="11919" :d="2" :w="2" multi>

`intptr_t` 与 `uintptr_t` 的类型是「能装下指针的整数类型」，具体对应 `int`、`long` 还是 `long long` 由实现决定。

(1) _[1.1]_ 运行下面的程序：

```c
#include <stdint.h>
#include <stdio.h>

int main(void) {
    int x = 42;
    uintptr_t p = (uintptr_t)&x;
    int *q = (int *)p;
    printf("%d %d %d\n", *q, q == &x, (int)sizeof(uintptr_t));
    return 0;
}
```

本机输出为 `42 1 8`。回答：`q == &x` 为 1 说明了什么，往返转换在什么前提下才有意义。

(2) _[3.2]_ 把 `uintptr_t` 换成 `unsigned long`，在 64 位 Windows 上编译运行。记录 `-Wpointer-to-int-cast`、`-Wint-to-pointer-cast` 两条警告，并解释程序为什么会在运行时崩溃。根因要从 64 位 Windows 上 `unsigned long` 的宽度说起。

(3) _[2.3]_ 把 `printf` 的 `%d` 换成 `"%" PRIxPTR`，宽度用 `2 * sizeof(uintptr_t)` 通过 `*` 传入。说明为什么地址列不该写死 16 位。

</Exercise>

### 9.2 编程

<Exercise id="11920" :d="4" :w="3">

把一段二进制数据按小端顺序读成整数、再写回去，是解析文件格式和网络报文的基本功。实现：

```c
/* 从 p 起读 n 个字节，按小端拼成整数。n 取 1～8。 */
uint64_t load_le(const uint8_t *p, size_t n);

/* 把 v 的低 n 个字节按小端写进 p。 */
void store_le(uint8_t *p, size_t n, uint64_t v);
```

要求：只用 `uint8_t` 与移位、按位或，不要 `memcpy` 到 `uint64_t` 对象上，也不要用联合体做类型双关。

验收：

| 调用 | 期望 |
| --- | --- |
| `load_le((const uint8_t[]){0x78, 0x56, 0x34, 0x12}, 4)` | `0x12345678` |
| `store_le(out, 8, UINT64_C(0x0123456789abcdef))` 后按 `%02x ` 打印 `out` | `ef cd ab 89 67 45 23 01` |

再回答：为什么不能写成 `*(uint64_t *)p`？把一个 `uint8_t` 数组直接转成 `uint64_t *` 解引用，在什么平台上会出错，错的又是什么。

</Exercise>

<Exercise id="11921" :d="4" :w="3" multi>

`uintptr_t` 最常见的用法之一，是把指针的数值单独拿出来做对齐计算。实现这三个函数：

```c
/* p 是否按 align 字节对齐。align 必须是 2 的幂。 */
int is_aligned(const void *p, size_t align);

/* 返回不大于 p 的最近对齐地址 */
void *align_down(void *p, size_t align);

/* 返回不小于 p 的最近对齐地址 */
void *align_up(void *p, size_t align);
```

(1) _[2.3]_ 用 `uintptr_t` 实现，不写除法也不用 `% align`（提示：`align - 1` 做掩码）。用 `char buf[64]`，令 `p = buf + 3` 测试：`is_aligned(p, 4)`、`is_aligned(p, 8)`、`align_down(p, 4) - buf`、`align_up(p, 8) - buf` 各应是多少？

(2) _[3.3]_ 写一组自动测试：对 `align` 取 1、2、4、8、16，`p` 从 `buf` 起每次加 1 走遍 64 个位置，断言 `align_down` 的结果对齐且不大于 `p`、`align_up` 的结果对齐且不小于 `p`，并且两者的差小于 `align`。用 `assert` 或自己比较都行。

(3) _[4.3]_ 把 `align_up` 先写成 `(uintptr_t)p + align - 1` 再掩码，说明当 `p` 接近 `UINTPTR_MAX` 时会发生什么。用 `UINTPTR_MAX - 2` 构造一个地址（不要解引用它），用 `"%" PRIxPTR` 打印输入与输出，记录结果。

(4) _[5.4*]_ 把三个函数改成接受任意 `align`（不要求是 2 的幂）的版本，不能再用掩码。给出实现，并说明为什么工程上通常宁愿在接口上约定「必须是 2 的幂」，也不做这个通用版本。

</Exercise>

<Exercise id="11922" :d="4" :w="3" multi>

写一个程序 `stdint_probe.c`，报告当前实现提供了哪些 `<stdint.h>` 类型，以及各极限宏的取值。输出形如：

```text
int8_t   -128..127
int16_t  -32768..32767
int32_t  -2147483648..2147483647
int64_t  9223372036854775807
intptr_t 8 字节
SIZE_MAX    18446744073709551615
PTRDIFF_MAX 9223372036854775807
```

(1) _[3.4]_ 用 `#ifdef INT8_MAX` 一类的条件编译逐项判断类型是否存在，用 `PRId8`、`PRId32`、`PRIuMAX` 一类宏打印，要求 `gcc -std=c17 -Wall -Wextra` 零警告。`SIZE_MAX`、`PTRDIFF_MAX` 先转成 `uintmax_t`、`intmax_t` 再打印。

(2) _[3.3]_ 用 `gcc -m32 -c` 交叉编译（本机没有 32 位运行库，只要编译通过）。再运行 `gcc -m32 -dM -E -include stdint.h - < /dev/null`，说明 32 位目标上哪几行的结果会与主机不同。

(3) _[4.3]_ 把 `printf` 里的 `PRId32` 全换成写死的 `%d`，用 `arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -c` 编译一次，记录 `-Wformat` 报出的问题。据此说明 `PRId*` 宏存在的理由。

</Exercise>

### 9.3 综合与挑战

<Exercise id="11923" :d="5" :w="4" multi>

读入一串十进制整数，用 `intmax_t` 累加，最后判断和是否落在 `int32_t` 范围内：

```text
输入：每行一个整数，读到 EOF 或非数字为止
输出（正常）：个数 和 是否在 int32_t 范围内（1 或 0）
失败：和超出 intmax_t 范围时向标准错误输出一条信息，退出码 1
```

要求：用 `SCNdMAX` 读、`PRIdMAX` 写；累加前先做溢出检查，不得让加法本身溢出。

(1) _[3.4]_ 完成程序。验收四组输入：

| 输入 | 期望 |
| --- | --- |
| `1 2 3` | `3 6 1` |
| `2147483647 1` | `2 2147483648 0` |
| `-9223372036854775808 1` | `2 -9223372036854775807 0` |
| `9223372036854775807 1` | 标准错误输出一条，退出码 1 |

(2) _[4.4]_ 写作时容易把检查写成 `if (sum + v > INTMAX_MAX)`。指出这样写错在哪、什么输入能触发，并给出正确的写法。

(3) _[4.4]_ 让程序同时接受十六进制：把 `SCNdMAX` 换成 `SCNiMAX`，用 `0x10` 与 `010` 各试一次，记录结果。解释 `SCNiMAX` 的转换规则与 `strtol` 的 `base` 参数取 0 时有什么相同之处。

(4) _[5.4*]_ 另写一份去掉检查的版本（直接 `sum += v`），用 `9223372036854775807 1` 运行，分别在带与不带 `-ftrapv` 时观察行为。再说明为什么这类检查不该依赖 `-ftrapv`，而要在加法之前自己判断。

</Exercise>

<Exercise id="11924" :d="6" :w="4" multi>

只要数据要落到字节流里，定宽类型就是必需品。设计一个小的报文头：

| 字段 | 类型 | 偏移 |
| --- | --- | --- |
| 序号 `seq` | `uint32_t` | 0 |
| 长度 `len` | `uint16_t` | 4 |
| 校验 `crc` | `uint32_t` | 8 |

总长 12 字节。

(1) _[3.3]_ 写出结构体 `struct wire`，用 `_Static_assert` 把总大小与三个偏移都固定下来（提示：`offsetof` 需要 `<stddef.h>`，C11 起可以直接写 `_Static_assert`）。在主机、`-m32` 与 Cortex-M3 上各编译一次，确认三条路径都通过。

(2) _[3.4]_ 给一条记录填 `{0x01020304u, 0x0506u, 0x0708090Au}`，把整个结构体按字节打印出来。在小端主机上应得到：

```text
04 03 02 01 06 05 00 00 0a 09 08 07
```

逐个字节说明它来自哪个字段的哪一部分，并指出哪两个字节是填充。

(3) _[4.4]_ 把 `seq`、`crc` 改成 `unsigned long`，在 64 位 Windows 上重新编译同样的断言。本机 `long` 只有 4 字节，断言照样通过；换到 LP64 的 Linux 上，同一份结构体会变成多大？再改成 `unsigned long long`，量出总大小与两个偏移，说明为什么跨机器的二进制格式不能用「基本整数类型」。

(4) _[5.5*]_ 改成不用结构体，直接用上一题的两个函数在 `uint8_t buf[12]` 上编解码，并在三种目标上确认 `sizeof(buf)` 与手工算出的偏移不变。回答：两种做法各自的可移植性代价是什么，为什么协议实现通常选后一种。

</Exercise>

<Exercise id="11925" :d="6" :w="4" multi>

哈希与校验这类代码对类型的宽度极其敏感。实现 FNV-1a 的 32 位版本：

```c
uint32_t fnv1a(const void *data, size_t n);
```

参数：FNV prime 为 `16777619`，offset basis 为 `2166136261`；每一步先异或一个字节，再乘 prime，全程按 32 位无符号回绕。

(1) _[2.3]_ 实现函数并用下面的测试向量验收：

| 输入 | 期望输出 |
| --- | --- |
| 空串（`n == 0`） | `811c9dc5` |
| `a` | `e40c292c` |
| `abc` | `1a47e90b` |
| `hello` | `4f9f2cab` |
| `The quick brown fox` | `ae4d67e2` |

(2) _[3.3]_ 把两个常量直接写成十进制字面量 `2166136261`、`16777619`，不加任何后缀，用 `_Generic`（见 [12.1 泛型选择](/教程/正文/语法和标准库/12_函数接口进阶/12_1_泛型选择)）判断它们的类型。本机上前者是 `long long`、后者是 `int`；在 `long` 为 64 位的平台上（如 64 位 Linux），前者会变成 `long`。说明为什么类型会随平台变化，以及为什么本例的类型差异不影响结果。

(3) _[3.3]_ 把累积对象 `h` 声明成 `int` 再运行同一组测试向量。本机上结果与 `uint32_t` 版本逐位相同，但这不能作为正确性的依据：乘法与异或的结果反复超出 `int` 的范围，属于未定义行为。用 `-ftrapv` 编译并运行，记录现象，再说明为什么「结果碰巧对」不能推广到别的编译器或优化级别。

(4) _[4.4]_ 用 `UINT32_C(...)` 重写常量，再对一个值为 `0x80` 的字节分别以 `const unsigned char *` 和 `const char *` 传入求值。两次结果必然不同（本机为 `850b939f` 与 `04f3b29f`）。解释差异来自哪一次转换，说明为什么哈希接口的形参必须写成 `const void *` 或 `const unsigned char *`。

(5) _[5.4*]_ 改成 64 位版本（prime 为 `1099511628211`，basis 为 `14695981039346656037`），用 `UINT64_C` 写常量。用空串、`a`、`hello` 验收，应得到 `cbf29ce484222325`、`af63dc4c8601ec8c`、`a430d84680aabd0b`。再把 `h` 的类型换成 `size_t`，在 32 位与 64 位目标上各跑一次（32 位目标只编译也行，用宏算出宽度），说明同一份源为什么会在两种目标上算出不同的值。

</Exercise>
