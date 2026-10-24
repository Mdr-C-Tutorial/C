# 19.3 浮点相关的宏 `float.h`

`<float.h>` 自 C89 起就是标准头文件，用宏给出浮点类型 `float`、`double`、`long double` 的特性参数，包括基数、精度、指数范围和极限值。这些值由实现决定，程序应使用这些宏，而不是假定 `FLT_MAX` 等于 `3.40282347e+38F`。

```c
#include <float.h>
```

## 1. 浮点类型特性宏

大多数宏按类型前缀区分：`FLT_`（`float`）、`DBL_`（`double`）、`LDBL_`（`long double`）。

- **基数和指数范围**
  - `FLT_RADIX`：浮点表示的基数（通常 2）。三种浮点类型共用这一个宏，没有 `DBL_RADIX`、`LDBL_RADIX`。
  - `_MANT_DIG`：尾数（mantissa）位数（含隐含位）。
    - 示例：`FLT_MANT_DIG`（通常 24）、`DBL_MANT_DIG`（通常 53）。
  - `_MIN_EXP`：最小负指数，使规范化浮点数不溢出（基数为 `FLT_RADIX`）。
  - `_MAX_EXP`：最大指数。
  - `_MIN_10_EXP`：最小负十进制指数（以 10 为底）。
  - `_MAX_10_EXP`：最大十进制指数。

- **精度**
  - `_DIG`：十进制有效数字位数（不丢失信息可表示的位数）。
    - 示例：`FLT_DIG`（通常 6）、`DBL_DIG`（通常 15）、`LDBL_DIG`（通常 18 或更多）。

- **范围极限**
  - `_MIN`：最小的正规范化浮点数（下溢阈值）。
    - 示例：`FLT_MIN`（通常 1.17549435e-38F）。
  - `_MAX`：最大浮点数（上溢阈值）。
    - 示例：`FLT_MAX`（通常 3.40282347e+38F）、`DBL_MAX`（通常 1.7976931348623157e+308）。
  - `_EPSILON`：1.0 与下一个可表示数之间的差（机器精度）。
    - 示例：`FLT_EPSILON`（通常 1.19209290e-7F）、`DBL_EPSILON`（通常 2.2204460492503131e-16）。

- **其他**
  - `FLT_ROUNDS`：浮点加法舍入模式（-1：不确定，0：向零，1：向最近，2：向正无穷，3：向负无穷）。
  - `_TRUE_MIN`（C11 及以后）：最小的正非规范化浮点数（如果支持渐变下溢）。
    - 示例：`FLT_TRUE_MIN`（通常 1.40129846e-45F）。
  - `_HAS_SUBNORM`（C11 及以后）：是否支持非规范化数（0：无，1：有）。

## 2. 宏示例（典型 IEEE 754 实现值）

| 宏             | 典型值（单精度 float） | 典型值（双精度 double） |
| -------------- | ---------------------- | ----------------------- |
| `FLT_RADIX`    | 2                      | 2                       |
| `FLT_MANT_DIG` | 24                     | —                       |
| `DBL_MANT_DIG` | —                      | 53                      |
| `FLT_DIG`      | 6                      | —                       |
| `DBL_DIG`      | —                      | 15                      |
| `FLT_MIN`      | 1.17549435e-38F        | —                       |
| `DBL_MIN`      | —                      | 2.2250738585072014e-308 |
| `FLT_MAX`      | 3.40282347e+38F        | —                       |
| `DBL_MAX`      | —                      | 1.7976931348623157e+308 |
| `FLT_EPSILON`  | 1.19209290e-7F         | —                       |
| `DBL_EPSILON`  | —                      | 2.2204460492503131e-16  |

## 3. 使用示例

```c
#include <float.h>
#include <stdio.h>

int main(void) {
    printf("float 的最大值: %e\n", FLT_MAX);
    printf("double 的最大值: %e\n", DBL_MAX);
    printf("float 的机器精度: %e\n", FLT_EPSILON);
    printf("十进制有效位数 (double): %d\n", DBL_DIG);
    return 0;
}
```

可能的输出：

::: terminal title="zsh - test_float"

float 的最大值: 3.402823e+38

double 的最大值: 1.797693e+308

float 的机器精度: 1.192093e-07

十进制有效位数 (double): 15

:::

## 4. 注意事项

- 这些宏是常量表达式。其中 `FLT_DIG`、`DBL_MANT_DIG` 等整数值的宏可以用在 `#if` 中；`FLT_MAX`、`FLT_EPSILON` 等浮点值的宏不能用于 `#if`。
- 实际值取决于实现（大多数现代系统遵循 IEEE 754，二进制基数、双精度 53 位尾数）。

## 5. 习题

### 5.1 巩固

<Exercise id="11930" :d="3" :w="3" multi>

`<float.h>` 的宏反映的是实现真正采用的格式，不是某张标准表格。先把本机的格式和几个关键参数打出来：

```c
#include <float.h>
#include <stdio.h>

int main(void)
{
#if FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128
    puts("float: binary32");
#else
    puts("float: 其他格式");
#endif
#if DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024
    puts("double: binary64");
#else
    puts("double: 其他格式");
#endif

    printf("radix = %d\n", FLT_RADIX);
    printf("float : p = %d, emin = %d, emax = %d, dig = %d\n",
           FLT_MANT_DIG, FLT_MIN_EXP, FLT_MAX_EXP, FLT_DIG);
    printf("float : min = %e, max = %e\n", FLT_MIN, FLT_MAX);
    printf("float : eps = %e, true_min = %e\n", FLT_EPSILON, FLT_TRUE_MIN);
    printf("double: p = %d, emin = %d, emax = %d, dig = %d\n",
           DBL_MANT_DIG, DBL_MIN_EXP, DBL_MAX_EXP, DBL_DIG);
    printf("double: min = %e, max = %e\n", DBL_MIN, DBL_MAX);
    printf("double: eps = %e, true_min = %e\n", DBL_EPSILON, DBL_TRUE_MIN);
    return 0;
}
```

本机输出：

```out
float: binary32
double: binary64
radix = 2
float : p = 24, emin = -125, emax = 128, dig = 6
float : min = 1.175494e-38, max = 3.402823e+38
float : eps = 1.192093e-07, true_min = 1.401298e-45
double: p = 53, emin = -1021, emax = 1024, dig = 15
double: min = 2.225074e-308, max = 1.797693e+308
double: eps = 2.220446e-16, true_min = 4.940656e-324
```

(1) _[2.2]_ 编译运行，核对自己的输出。然后回答：`FLT_MIN` 是 $2^{-126}$，为什么 `FLT_MIN_EXP` 是 -125 而不是 -126？用 `FLT_MAX_EXP` 写出 `FLT_MAX` 的表达式 $(1-2^{-p})\times 2^{e_{\max}}$，算出具体值，与程序打印的 `max` 对照。

(2) _[2.3]_ 把两个 `#if` 改写成用浮点宏判断，编译通不过：

```c
#if FLT_MAX > 1.0e30
#define WIDE_RANGE 1
#else
#define WIDE_RANGE 0
#endif

#if DBL_MANT_DIG == 53.0
#define BINARY64 1
#else
#define BINARY64 0
#endif
```

抄下 GCC 报出的错误原文，说明预处理阶段为什么不能用浮点常量，再改成结论相同、能通过预处理的写法。

(3) _[3.3]_ 从 `FLT_MIN` 出发反复除以 2，直到结果为 0，数出一共除了多少次，并打出最后一个非零值。把它与 `FLT_TRUE_MIN`、`FLT_MANT_DIG` 对照，说明 23 这个数从哪里来，为什么再除一次就变成 0。

</Exercise>

### 5.2 编程

<Exercise id="11931" :d="4" :w="3">

`<float.h>` 给出的 p、emin、emax 三个整数足以确定另外四个范围宏。先写一个只做乘除 2 的缩放函数：

```c
/* 返回 x * 2^k，k 为正表示乘，为负表示除 */
double scale2(double x, int k);
```

用它算出下表中用参数表示的值，与 `float`、`double` 的同名宏逐一比较，相等打印 `ok`，否则打印 `bad`：

| 宏 | 用 p、emin、emax 表示 |
| --- | --- |
| `_MAX` | $(2^{p}-1)\cdot 2^{e_{\max}-p}$ |
| `_MIN` | $2^{e_{\min}-1}$ |
| `_EPSILON` | $2^{1-p}$ |
| `_TRUE_MIN` | $2^{e_{\min}-p}$ |

然后换一种写法，先算 $(1-2^{-p})$ 再乘 $2^{e_{\max}}$，同样与 `_MAX` 比较，并打印 $2^{e_{\max}}$ 本身。

要求：

1. `scale2` 只用乘法与除法，不调用 `pow`、`ldexp`；
2. 对 `float` 与 `double` 各检查四个宏，相等打印 `ok`，否则打印 `bad`；
3. 两种 `_MAX` 写法都比一遍。

本机输出：

```out
FLT_MAX        ok
FLT_MIN        ok
FLT_EPSILON    ok
FLT_TRUE_MIN   ok
DBL_MAX        ok
DBL_MIN        ok
DBL_EPSILON    ok
DBL_TRUE_MIN   ok
FLT_MAX/2^emax ok
DBL_MAX/2^emax bad
2^128 = 3.40282e+38, 2^1024 = inf
```

最后解释：同样的第二种写法，为什么对 `float` 成立，对 `double` 却算出 `inf`。表中第一种写法为什么不会溢出，两种写法在数值上是否等价。

</Exercise>

<Exercise id="11932" :d="4" :w="4" multi>

`FLT_DIG` 与 `FLT_DECIMAL_DIG` 都表示十进制位数，作用却不同。用实验把两者区分开。先写两个往返函数：

```c
#include <float.h>
#include <stdio.h>

/* 用 digits 位有效数字写出 v 再读回，相等返回 1 */
int roundtrip_float(float v, int digits)
{
    char buf[64];
    float back = 0.0f;
    snprintf(buf, sizeof buf, "%.*g", digits, (double)v);
    if (sscanf(buf, "%f", &back) != 1) {
        return 0;
    }
    return back == v;
}

int roundtrip_double(double v, int digits)
{
    char buf[64];
    double back = 0.0;
    snprintf(buf, sizeof buf, "%.*g", digits, v);
    if (sscanf(buf, "%lf", &back) != 1) {
        return 0;
    }
    return back == v;
}
```

(1) _[3.3]_ 用 `FLT_DIG` 与 `FLT_DECIMAL_DIG` 分别测 `0.1f`、`16777216.0f`、`1.2345678f`、`3.14159265f`；用 `DBL_DIG` 与 `DBL_DECIMAL_DIG` 测 `0.1`、`1.0 / 3.0`、`123456789.123456789`、`1e-300`。把结果填入下表：

| 值 | 用 `_DIG` 位 | 用 `_DECIMAL_DIG` 位 |
| --- | --- | --- |
| `0.1f` |  |  |
| `16777216.0f` |  |  |
| `1.2345678f` |  |  |
| `3.14159265f` |  |  |
| `0.1` |  |  |
| `1.0 / 3.0` |  |  |
| `123456789.123456789` |  |  |
| `1e-300` |  |  |

验收：`_DIG` 列有 5 行是 `diff`，`_DECIMAL_DIG` 列 8 行全部是 `ok`。

再对每个 float 值从 1 位开始试，找出刚好能往返的最少位数。`0.1f` 只需要 1 位，这与 `FLT_DIG` 等于 6 矛盾吗？两个宏各自保证的是什么？

(2) _[4.4]_ 写 `int nearly_equal(double a, double b, double tol)`，按相对容差判断：两数之差的绝对值不超过 `tol` 乘以 `|a|` 与 `|b|` 中较大的一个；两者都为 0 时返回 1。用 `tol = 4 * DBL_EPSILON` 判断下面五组，给出结果：

```out
0.1 + 0.2 与 0.3          1
1 与 1 + 4 * eps          1
1 与 1 + 5 * eps          0
1e-300 与 0               0
1e300 与 1e300 * (1 + eps) 1
```

说明 `1e-300` 与 0 为什么被判为不等，以及这种相对容差在什么情况下会给出误导性的结论。

(3) _[4.4]_ 写 `float ulp_at(float x)`：从 `FLT_EPSILON` 开始不断翻倍，直到 `x + e != x`，返回这个 e。对 `1.0f`、`2.0f`、`1e6f` 各测一次，本机得到 `1.192093e-07`、`2.384186e-07`、`6.250000e-02`。解释三者的比例关系，并说明在 1e6 附近 `x + FLT_EPSILON == x` 的原因。

</Exercise>

### 5.3 综合与挑战

<Exercise id="11933" :d="5" :w="4" multi>

`0.1f` 在 float 里并不等于 0.1。把它加一百万次，本机得到 100958.344，比 100000 多出近 1000；加到一亿次，结果停在 2097152 不再变化。本题把误差量化，并比较三种累加方式。

(1) _[2.3]_ 写程序，对 n = 1000000 与 n = 100000000 各做三种累加：

1. `float` 直接累加；
2. `double` 累加，每次仍加 `0.1f`；
3. Kahan 补偿求和，全部用 `float`，每步为：

```c
float y = 0.1f - c;
float t = sum + y;
c = (t - sum) - y;
sum = t;
```

以 `(double)0.1f * n` 为参考值，打印结果与相对误差。本机输出：

```out
n = 1000000
  float  100958.344  rel +9.583e-03
  double 100000.001490  rel +0.000e+00
  kahan  100000.000  rel -1.490e-08
n = 100000000
  float  2097152.000  rel -7.903e-01
  double 10000000.149012  rel +0.000e+00
  kahan  10000000.000  rel -1.490e-08
```

(2) _[4.3]_ 解释 n = 1e8 时 float 的结果为什么恰好是 $2^{21}$。在 $[2^{21}, 2^{22})$ 内 float 的 ulp 是多少？`0.1f` 与半个 ulp 哪个大？为什么此后的每次加法都不改变这个和？

(3) _[4.4]_ Kahan 求和停在 -1.49e-8，原因不在算法。验证 $10^{7}$ 附近 float 的 ulp 是 1，并用它解释：精确到每一小步的补偿求和，最后仍要把 10000000.149 舍入到 float 能表示的值，误差就来自这一步。

(4) _[5.4*]_ 上面三种累加都以 `(double)0.1f * n` 为参考值。这一问改用 `long double` 的 `(long double)0.1f * n` 作参考，它连最后一位都不舍入，重新观察 double 累加：

1. 二分找出累加和第一次超过 $2^{26} = 67108864$ 的 n，本机是 671088631。此时 ulp 从 $2^{-27}$ 跳到 $2^{-26}$。
2. 从 n = 671088631 起，把每个 n 的误差折成 $2^{-27}$ 的倍数，本机依次是 +1、0、-1、-2、-3，到 $n = 10^9$ 时是 -328911368。验证这个倍数等于 $671088632 - n$。
3. 在部分和小于 $2^{26}$ 的区间里误差恒为 0。`0.1f` 的精确值是 $13421773 \times 2^{-27}$，说明为什么这一段每次加法都落在可表示值上。
4. 进入 $[2^{26}, 2^{27})$ 后 ulp 是 $2^{-26}$，可表示值是 $2^{-27}$ 的偶数倍，而 `0.1f` 是 $13421773$ 个 $2^{-27}$，奇数倍。实测从 n = 671088632 起，误差每步固定变化一个 $2^{-27}$。用「和是偶数倍、加数是奇数倍」这一条解释为什么每次加法的精确结果都不可能落在可表示值上，再说明舍入为什么每次都选同一侧，使误差单向累积而不正负抵消。

最后从 n = 7e8 取到 1e9，每隔 1e8 测一次累加结果与 `(double)0.1f * n` 之差，本机累加结果始终偏小，差额依次为 -0.215406、-0.960465、-1.70552、-2.45058。用第 2 步的公式核对这四个数。

</Exercise>
