# 19.5 复数库 `<complex.h>`

## 1. 复数和虚数类型

- `double _Complex` `float _Complex` `long double _Complex`
- `double _Imaginary` `float _Imaginary` `long double _Imaginary`

如果包含头文件 `<complex.h>`，则上面类型可以写作

- `double complex` `float complex` `long double complex`
- `double imaginary` `float imaginary` `long double imaginary`

`imaginary` 和 `complex` 前面需要加一个 **类型说明符**，限定是 `float`，`double` 还是 `long double`。

`+` `-` `*` `/` 运算符对于实数和虚数以及它们的混合均适用。
因为 $\mathbb{C}$ 不是有序域，所以不支持 `>` `<` `>=` `<=` 运算符。

::: info 编译器支持

目前所有主流编译器均不支持 `_Imaginary` 类型（截至 2024 年 11 月 17 日）。

:::

## 2. 构建复数

### 2.1 宏 `I`

虚数单位是 $i$，但考虑到 `i` 是个经常使用的标识符（如在循环当中），所以用 `I` 代替。

如果 `_Imaginary` 类型已定义，它会展开成 `const float _Imaginary` 类型的值；否则，它会展开成 `const float _Complex` 类型的值。它的值是虚数单位。

```c
#include <complex.h>

double complex a = 3.4 + 1.54*I;
float complex b = 3.3*I;
```

### 2.2 宏 `CMPLXF` `CMPLX` `CMPLXL`

这三个是宏，但是行为像下面这仨函数：

```c
float complex       CMPLXF( float real, float imag );
double complex      CMPLX( double real, double imag );
long double complex CMPLXL( long double real, long double imag );
```

宏可能是这么实现的（不同编译器上实现可能略有不同，但行为和下面代码相同）：

```c
#define CMPLX(x, y) ((double complex)((double)(x) + _Imaginary_I * (double)(y)))
#define CMPLXF(x, y) ((float complex)((float)(x) + _Imaginary_I * (float)(y)))
#define CMPLXL(x, y) ((long double complex)((long double)(x) + \
                      _Imaginary_I * (long double)(y)))
```

示例：

```c
#include <complex.h>

double complex a = CMPLX(3.2, 4.5);
float complex b = CMPLXF(3.2, 4.5);
long double complex c = CMPLXL(3.2, 4.5);
```

## 3. 主要操作

对于复数的三个类型，每种操作都提供三个不同的函数：对于 `double _Complex` 的函数名最短，对于 `float _Complex` 的函数名是前者尾部加 **f**；对于 `long double _Complex` 的函数名是前者尾部加 **l**，函数的形参和返回值类型也进行相应的替换。

为了节省篇幅，只列出对于 `double _Complex` 的函数。

### 3.1 求实部和虚部

```c
double creal( double _Complex z ); // 返回实部
double cimag( double _Complex z ); // 返回虚部
```

对于任何类型的复数 `z`，`creal(z) + I * cimag(z)` 都等于 `z`。

### 3.2 求模和幅角

```c
double cabs( double _Complex z ); // 求模
double carg( double _Complex z ); // 求幅角
// 注意幅角返回的是弧度
```

### 3.3 求共轭

```c
double _Complex conj( double _Complex z ); // 共轭
```

## 4. 其他函数

在 `<math.h>` 里面的函数，在 `<complex.h>` 里面普遍有对应的版本，函数名即为 `<math.h>` 里面函数名前面加一个 **c**。如下示例：

```c
float  sinf( float  arg );
double sin ( double arg );

float  complex csinf( float  complex z );
double complex csin ( double complex z );
```

## 5. 习题

### 5.1 巩固

<Exercise id="11934" :d="2" :w="2">

下面这段程序有三处问题。GCC 对其中一处只给警告，另外两处直接报错：

```c
#include <complex.h>
#include <stdio.h>

int main(void)
{
    double complex z = 3.0 + 4.0 * I;

    printf("%f\n", z);

    if (z > 1.0) {
        puts("larger");
    }

    for (int I = 0; I < 3; I++) {
        puts("tick");
    }

    return 0;
}
```

(1) 逐处抄下 `gcc -std=c17 -Wall -Wextra` 的警告或报错原文，指出每一处违反了本节的哪条规则。`printf` 那一处为什么只是警告，而 `>` 那一处是错误？

(2) 只改必要的部分，让程序零警告编译并输出：

```out
3.000000 4.000000
larger
tick
tick
tick
```

(3) `I` 被宏占用的问题，除了把循环里那个名字换掉，还可以在包含头文件之后用一条预处理指令解决。写出这条指令，并说明它有什么副作用。

</Exercise>

<Exercise id="11935" :d="3" :w="3">

写一个复数计算器：从标准输入读入四个实数 `re1 im1 re2 im2`，按 $z_1 = re_1 + im_1 i$、$z_2 = re_2 + im_2 i$ 构造，输出和、差、积、商，以及 $|z_1|$、$\arg z_1$。

按下面的格式输出（数值为输入 `3 4 1 -2` 时的本机结果）：

```out
sum  = 4.0000 +2.0000i
diff = 2.0000 +6.0000i
prod = 11.0000 -2.0000i
quot = -1.0000 +2.0000i
|z1| = 5.000000, arg(z1) = 0.927295
```

要求：

1. 只用 `+`、`-`、`*`、`/` 完成四则运算，不要展开成实部虚部分别计算；
2. 模与幅角用 `cabs`、`carg`，幅角以弧度输出；
3. 读入失败时返回非零值；
4. 末尾再打印一行 `|z1*z2| - |z1||z2|`，用 `%.3e` 输出，这一行应为 `0.000e+00`。

验收：再输入 `3 4 0 0`（$z_2 = 0$），程序不应崩溃，商变成 `inf +infi`，说明是哪一步产生的。再输入 `0 0 1 0`（$z_1 = 0$），商的两个分量都是 0，与上一种情况对比，说明结果取决于除数而不是被除数。

</Exercise>

### 5.2 编程

<Exercise id="11936" :d="6" :w="4" multi>

`creal(z) + I * cimag(z)` 等于 `z`。反过来也被大量使用：写 `re + im * I` 来拼一个复数。这种做法在实参是普通有限数时没有问题，遇到无穷、NaN 和负零就会出岔子。

(1) _[2.2]_ 先确认拆开再拼回不改变值：写程序对 `3.0 + 4.0 * I`、`-1.0 + 0.0 * I`、`0.0 - 2.5 * I` 各做一次 `creal(z) + I * cimag(z)`，用 `==` 比较，本机三次都是 1。

(2) _[3.3]_ 用 `re + im * I` 求方程 $ax^2 + bx + c = 0$ 的两个复根，判别式开方用 `csqrt(b*b - 4*a*c)`。对下面四组系数运行：

| `a` | `b` | `c` |
| --- | --- | --- |
| 1 | 2 | 5 |
| 1 | -2 | 1 |
| 3 | 1 | 2 |
| 1 | -3 | 2 |

用 `%.4f` 和 `%+.4f` 输出实部虚部，本机依次为：

```out
x1 = -1.0000 +2.0000i, x2 = -1.0000 -2.0000i
x1 = 1.0000 +0.0000i, x2 = 1.0000 -0.0000i
x1 = -0.1667 +0.7993i, x2 = -0.1667 -0.7993i
x1 = 2.0000 +0.0000i, x2 = 1.0000 -0.0000i
```

把每个根代回原方程，用 `cabs` 求残差：第一、二、四组是 0，第三组是 `2.220e-16`。说明第二组的 `-0.0000` 从哪里来，它和第三组的 `2.220e-16` 是不是一回事，以及 `-0.0000` 会不会让 `x2 == 1.0` 判为假。

(3) _[4.4**]_ 用 `<math.h>` 的 `NAN`、`INFINITY` 各造一个 `double`，分别按 `1.0 + nan * I`、`1.0 + inf * I` 构造复数，打出实部与虚部。再写 `double complex make_complex(double re, double im)`，用 `re + I * im` 实现，对下表三组实参各构造一次，并与 GCC 扩展的内建函数 `__builtin_complex(re, im)`（直接把两个同类型的浮点数拼成复数，不做乘加）对比，输出 `isnan(creal)`、`isnan(cimag)`、`signbit(cimag)`：

| 实部 | 虚部 |
| --- | --- |
| 1.0 | `INFINITY` |
| 2.0 | `NAN` |
| 1.0 | `-0.0` |

本机结果：

```out
make(1,inf)     1 0 0     __builtin_complex(1,inf)   0 0 0
make(2,nan)     1 1 0     __builtin_complex(2,nan)   0 1 0
make(1,-0)      0 0 1     __builtin_complex(1,-0)    0 0 1
```

前两行里 `make_complex` 的实部变成了 NaN，`__builtin_complex` 的实部仍是传进去的数。`NAN` 与 `INFINITY` 都是 `float` 类型的宏，调用 `__builtin_complex` 时要写成 `(double)INFINITY`、`(double)NAN`，两个实参类型不一致会编译不过。解释 `im * I` 做了什么、为什么连实部都被改写。

(4) _[4.4*]_ 写一个只用标准 C 的替代实现：

```c
/* 把 re、im 拼成 double complex，不做任何算术运算 */
double complex make_complex_bits(double re, double im);
```

提示：把两个 `double` 放进数组，用 `memcpy` 拷进一个 `double complex` 对象。用上一问的六组数据验证它与 `__builtin_complex` 逐分量一致，并打印 `(int)sizeof(double complex)` 确认它等于 `2 * sizeof(double)`。C11 起标准规定复数类型与「两个对应实数元素构成的数组」具有相同的表示和对齐要求，实部在前、虚部在后，所以这个写法本身是可移植的；请据此说明这个实现为什么成立，以及为什么它不能用于静态初始化和常量表达式。

(5) _[6.4*]_ 把本节给出的 `CMPLX` 宏原样抄进程序：

```c
#define CMPLX(x, y) ((double complex)((double)(x) + _Imaginary_I * (double)(y)))
```

它在本机编译不过，报 `'_Imaginary_I' undeclared`，换成 `I` 之后才能用。先解释原因。再把 `(double)y` 的括号去掉：

```c
#define CMPLX_BAD(x, y) ((double complex)((double)(x) + I * (double)y))
```

用 `CMPLX_BAD(3.0, 1.0 + 2.0)` 调用，观察结果从 `3.0 + 3.0i` 变成 `5.0 + 1.0i`。说明标准为什么把 `CMPLX` 定为宏而不是函数，以及宏参数必须整体加括号这条规则在这里具体落在哪一次运算上。

</Exercise>

### 5.3 综合与挑战

<Exercise id="11937" :d="6" :w="5" multi>

离散傅里叶变换（DFT）是复数最典型的工程用途。对长度 $N = 8$ 的实序列 $x_n = \cos(2\pi \cdot 3n / 8)$，定义

$$
X_k = \sum_{n=0}^{N-1} x_n e^{-2\pi i k n / N}, \qquad k = 0, 1, \dots, N-1
$$

(1) _[3.3]_ 用 `cexp` 构造旋转因子，按定义算出全部 $X_k$，打印每个 $|X_k|$。本机输出中只有 $k = 3$ 与 $k = 5$ 的模为 4，其余都是 0 量级。解释这两个位置对应的频率，以及为什么总是对称地出现两个。

(2) _[4.4]_ 打印每个 $X_k$ 的实部与虚部。本该为 0 的分量是 $10^{-16}$ 量级的残值而非精确的 0。对这批数据验证共轭对称性 $\max_k |X_{(N-k) \bmod N} - \overline{X_k}|$，本机约为 `3.501e-15`。这个量应当用 `==` 判断吗？

(3) _[5.4]_ 把输入换成 $x_n = \sin(2\pi \cdot 3n / 8)$ 重跑。模仍然是 $k = 3, 5$ 处为 4，但幅角不同：本机为 `-1.570796` 与 `+1.570796`。说明正弦与余弦的谱只差一个相位，并解释这两个位置的幅角为什么互为相反数。

(4) _[6.4*]_ 验证 Parseval 定理 $\sum_k |X_k|^2 = N \sum_n |x_n|^2$。用 `creal(X[k] * conj(X[k]))` 求 $|X_k|^2$，比较两边的相对误差（$N = 8$ 时本机为 `6.661e-16`）。把 $N$ 依次改成 16、32、64、128，输入频率固定为每周期 3 个，记录相对误差（本机依次为 `0`、`2.220e-16`、`9.992e-16`、`1.776e-15`）。误差是否随 $N$ 单调增大？它来自求和次序、`cexp` 的残值，还是两者都有？

</Exercise>
