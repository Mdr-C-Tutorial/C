# C 复数库(C Complex Library)

## 1. 复数和虚数类型

+ `double _Complex` `float _Complex` `long double _Complex`
+ `double _Imaginary` `float _Imaginary` `long double _Imaginary`

如果包含头文件 `<complex.h>`，则上面类型可以写作

+ `double complex` `float complex` `long double complex`
+ `double imaginary` `float imaginary` `long double imaginary`

`imaginary` 和 `complex` 前面需要加一个**类型限定符**，限定是 `float`，`double` 还是 `long double`

`+` `-` `*` `/` 运算符对于实数和虚数以及它们的混合均适用。
因为 $\mathbb{C}$ 不是有序域，所以不支持 `>` `<` `>=` `<=` 运算符。

> 注：目前所有主流编译器均**不**支持 `_Imaginary` 类型

## 2. 构建复数

### 2.1 宏 `I`

虚数单位是 $i$，但考虑到 `i` 是个经常使用的标识符（如在循环当中），所以用 `I` 代替。

如果 `_Imaginary` 类型已定义，它会展开成 `const float _Imaginary` 类型的值，值为虚数单位；否则（目前来讲**绝大多数**情况），它会展开成 `const float _Complex` 类型的值，（值也为虚数单位）。

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

宏是这么实现的（不同编译器上实现可能略有不同，但行为和下面代码相同）：

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

对于复数的三个类型，每种操作都提供三个不同的函数：对于 `double _Complex` 的函数名最短，对于 `float _Complex` 的函数名是前者尾部加 **f**；对于 `long double _Complex` 的函数名是前者尾部加 **l**。
函数的形参和返回值类型也进行相应的替换。
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
double conj( double _Complex z ); // 共轭
```

## 4. 其他函数

在 `<math.h>` 里面的函数，在 `<complex.h>` 里面普遍有对应的版本，函数名即为 `<math.h>` 里面函数名前面加一个 **c**。如下示例：

```c
float  sinf( float  arg );
double sin ( double arg );

float  complex csinf( float  complex z );
double complex csin ( double complex z );
```
