# 19.2 C语言标准库头文件：limits.h

`<limits.h>` 自 C89 起就是标准头文件，定义字符类型和标准整数类型的取值范围。这些宏的值由实现决定，程序应使用这些宏，而不是假定 `INT_MAX` 等于 `2147483647`。

```c
#include <limits.h>
```

## 1. 字符类型极限

- `CHAR_BIT`：一个字节中的位数（通常 8，至少 8）。
- `SCHAR_MIN`、`SCHAR_MAX`：`signed char` 的最小值和最大值，通常为 -128 和 127。
- `UCHAR_MAX`：`unsigned char` 的最大值（通常 255）。
- `CHAR_MIN`、`CHAR_MAX`：`char` 的最小值和最大值。若 `char` 按有符号处理，二者分别等于 `SCHAR_MIN` 与 `SCHAR_MAX`；否则 `CHAR_MIN` 为 0，`CHAR_MAX` 等于 `UCHAR_MAX`。
- `MB_LEN_MAX`：一个多字节字符的最大字节数（至少 1）。

## 2. 整数类型极限

这些宏对应标准整数类型的最小和最大值：

- 短整型：
  - `SHRT_MIN`、`SHRT_MAX`：`signed short`（至少 -32767 ~ 32767）。
  - `USHRT_MAX`：`unsigned short`（至少 65535）。

- 整型：
  - `INT_MIN`、`INT_MAX`：`signed int`（至少 -32767 ~ 32767，通常 -2147483648 ~ 2147483647）。
  - `UINT_MAX`：`unsigned int`（至少 65535，通常 4294967295）。

- 长整型：
  - `LONG_MIN`、`LONG_MAX`：`signed long`（至少 -2147483647 ~ 2147483647）。
  - `ULONG_MAX`：`unsigned long`（至少 4294967295）。

- 长长整型（C99 及以后）：
  - `LLONG_MIN`、`LLONG_MAX`：`signed long long`（至少 -9223372036854775807 ~ 9223372036854775807）。
  - `ULLONG_MAX`：`unsigned long long`（至少 18446744073709551615）。

## 3. 使用示例

```c
#include <limits.h>
#include <stdio.h>

int main(void) {
    printf("int 的最小值: %d\n", INT_MIN);
    printf("int 的最大值: %d\n", INT_MAX);
    printf("unsigned int 的最大值: %u\n", UINT_MAX);
    printf("一个字节的位数: %d\n", CHAR_BIT);
    return 0;
}
```

可能的输出：

::: terminal title="zsh - test_limits"

int 的最小值: -2147483648

int 的最大值: 2147483647

unsigned int 的最大值: 4294967295

一个字节的位数: 8

:::

## 4. 习题

### 4.1 巩固

<Exercise id="11926" :d="1" :w="2" multi>

下面程序里，`char` 是否按有符号处理决定了结果：

```c
#include <limits.h>
#include <stdio.h>

int main(void) {
    char c = 200;
    unsigned char u = 200;

    printf("%d %d %d %d\n", CHAR_BIT, (int)sizeof(char), CHAR_MIN, CHAR_MAX);
    printf("%d %d\n", (int)c, (int)u);
    printf("%d %d\n", c == 200, u == 200);
    return 0;
}
```

(1) _[1.2]_ 先写出预测再运行。本机输出为 `8 1 -128 127`、`-56 200`、`0 1`。

(2) _[2.2]_ 用 `-funsigned-char` 再编译一次，抄下输出，并解释 `CHAR_MIN`、`CHAR_MAX` 为什么一起变了。编译时出现的 `comparison is always false due to limited range of data type` 警告指的是哪一边。

(3) _[2.3]_ 把 `char c = 200;` 改成 `char c = (char)200;`，重新编译。警告是否消失？输出是否改变？说明显式转换在这里改变了什么、没改变什么。

</Exercise>

<Exercise id="11927" :d="2" :w="2" multi>

`<limits.h>` 的宏之间有可以互相推出来的关系。写程序验证下面几条在本机是否成立：

1. `UCHAR_MAX + 1u == 1u << CHAR_BIT`；
2. `SCHAR_MAX == UCHAR_MAX / 2`；
3. `INT_MAX == (int)(UINT_MAX / 2)`；
4. `SHRT_MAX == (short)(USHRT_MAX / 2)`。

(1) _[1.1]_ 把四条各写成一条 `printf`，打印布尔结果。本机四条全为 1。

(2) _[2.2]_ 再加一条：`(long long)INT_MAX * 2 + 1 == UINT_MAX`。为什么这里必须先转成 `long long`？直接写 `INT_MAX * 2 + 1` 编译时会出现什么警告，运行时结果是否碰巧仍为真？

(3) _[3.3]_ 用 `CHAR_BIT * sizeof(类型)` 打印 `char`、`short`、`int`、`long`、`long long` 的位数。为什么不能直接写 8、16、32？把这条写成一个宏 `BIT_WIDTH(type)`，并说明宏体里为什么每一处括号都不能省。

</Exercise>

### 4.2 编程

<Exercise id="11928" :d="4" :w="3" multi>

写一个 `limits_probe.c`，把 `<limits.h>` 的宏整理成一张表输出：

```text
char                8 位  -128..127
unsigned char       8 位  0..255
short              16 位  -32768..32767
unsigned short     16 位  0..65535
int                32 位  -2147483648..2147483647
unsigned int       32 位  0..4294967295
long               32 位  -2147483648..2147483647
unsigned long      32 位  0..4294967295
long long          64 位  -9223372036854775808..9223372036854775807
unsigned long long 64 位  0..18446744073709551615
```

(1) _[3.4]_ 实现它。位宽一列用 `CHAR_BIT * sizeof` 算，取值全部来自宏，`gcc -std=c17 -Wall -Wextra` 零警告。本机输出如上（`long` 与 `int` 都是 32 位）。

(2) _[3.4]_ 加一组编译期检查：把标准规定的最小范围（`short` 至少 16 位、`int` 至少 16 位、`long` 至少 32 位、`long long` 至少 64 位）写成 `_Static_assert`，例如 `_Static_assert(SHRT_MAX >= 32767, "short too narrow");`。编译通过即说明实现满足下界；在主机、`-m32` 与 Cortex-M3 上各编译一次。

(3) _[4.4]_ 用一个 `#if` 判断当前目标上 `long` 是否比 `int` 宽，输出 `long 比 int 宽` 或 `long 与 int 同宽`。本机两个目标都输出后者。再用 `gcc -m32 -E` 预处理同一份文件，确认结论相同与否。

(4) _[5.4*]_ 把程序改成基本不依赖 `<limits.h>`：无符号类型的最大值用 `(T)-1` 得到，`int` 的最大值用 `(unsigned)-1 >> 1` 得到（在常见实现上），只有 `CHAR_BIT` 仍然取自头文件。运行结果与第 (1) 问逐项对比，指出哪些值推不出来。再说清两件事：对无符号类型 `(T)-1` 为什么总有定义；对 `char` 为什么推不出 `CHAR_MIN`（提示：`char` 是否有符号由实现决定，而 `char` 与 `signed char`、`unsigned char` 是三个不同类型）。

</Exercise>

### 4.3 综合与挑战

<Exercise id="11929" :d="5" :w="4" multi>

下面的程序有三处依赖「`int` 的极限」，每处都有问题：

```c
#include <limits.h>
#include <stdio.h>

static long long sum(int a, int b) {
    long total = a + b;
    return total;
}

static int midpoint(int a, int b) {
    return (a + b) / 2;
}

static int abs_int(int v) {
    return v < 0 ? -v : v;
}

int main(void) {
    printf("%lld\n", sum(INT_MAX, 1));
    printf("%d\n", midpoint(INT_MAX, INT_MAX));
    printf("%d\n", abs_int(INT_MIN));
    return 0;
}
```

(1) _[3.3]_ 编译运行，本机三行输出为 `-2147483648`、`-1`、`-2147483648`。逐个指出哪一行是错的、错在哪个表达式，以及为什么 `sum` 里把结果存进 `long` 并不能解决问题。

(2) _[4.4]_ 三处都改成不产生有符号溢出的写法，要求对 `INT_MAX`、`INT_MIN` 都给出正确结果。验收输出：

```text
2147483648
2147483647
2147483648
```

(3) _[4.4]_ 用 `gcc -ftrapv` 重新编译原程序并运行，记录现象（本机是异常终止，退出码非 0）。`-ftrapv` 能让这份程序停下来，但它能不能当作可靠的溢出检查手段？从标准对整数溢出的规定出发回答。

(4) _[5.4]_ 把 `midpoint` 与 `abs_int` 的实参改成从命令行读入的 `int`（用 `strtol` 解析并检查转换结果），分别传入 `2147483647` 与 `-2147483648`，记录输出。本机 `midpoint` 打印 `-1`、`abs_int` 打印 `-2147483648`，两个返回值都是错的。用这个现象说明为什么「用 `long` 接结果」「先算后判」这类写法不能替代真正的溢出检查，并给出一个不依赖运算结果是否已经溢出的正确判据。

</Exercise>
