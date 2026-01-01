# 浮点相关的宏 `float.h`

`<float.h>` 是C语言标准库中的头文件，从最早的标准起就存在，在C23（ISO/IEC 9899:2024）中内容基本未变。它定义了浮点类型（`float`、`double`、`long double`）的实现特性宏，包括表示范围、精度、舍入模式等。这些宏的值由实现决定，用于编写可移植的浮点代码（避免硬编码假设如 `FLT_MAX = 3.40282347e+38F`）。

使用方式：

```c
#include <float.h>
```

## 1. 浮点类型特性宏

宏按类型前缀区分：`FLT_`（`float`）、`DBL_`（`double`）、`LDBL_`（`long double`）。

- **基数和指数范围**
  - `_RADIX`：浮点表示的基数（通常 2）。
    - 示例：`FLT_RADIX`、`DBL_RADIX`、`LDBL_RADIX`（大多数实现均为 2）。
  - `_MANT_DIG`：尾数（mantissa）位数（不含隐含位）。
    - 示例：`FLT_MANT_DIG`（通常 24）、`DBL_MANT_DIG`（通常 53）。
  - `_MIN_EXP`：最小负指数，使规范化浮点数不溢出（基数为 `_RADIX`）。
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
  - `_ROUNDS`：浮点加法舍入模式（-1：不确定，0：向零，1：向最近，2：向正无穷，3：向负无穷）。
  - `_TRUE_MIN`（C99及以后）：最小的正非规范化浮点数（如果支持渐变下溢）。
    - 示例：`FLT_TRUE_MIN`（通常 1.40129846e-45F）。
  - `_HAS_DENORM`（C99及以后）：是否支持非规范化数（0：无，1：有）。
  - `_HAS_INFINITY`、`_HAS_QUIET_NAN` 等（C99及以后）：是否支持无穷和NaN。

## 2. 宏示例（典型IEEE 754实现值）

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

## 使用示例

```c
#include <float.h>
#include <stdio.h>

int main() {
    printf("float 的最大值: %e\n", FLT_MAX);
    printf("double 的最大值: %e\n", DBL_MAX);
    printf("float 的机器精度: %e\n", FLT_EPSILON);
    printf("十进制有效位数 (double): %d\n", DBL_DIG);
    return 0;
}
```

可能的输出：

<TerminalWindow title="zsh - test_float">

float 的最大值: 3.402823e+38

double 的最大值: 1.797693e+308

float 的机器精度: 1.192093e-07

十进制有效位数 (double): 15

</TerminalWindow>

## 注意事项

- 这些宏是常量表达式，可用于预处理或静态断言。
- 实际值取决于实现（大多数现代系统遵循IEEE 754，二进制基数、双精度53位尾数）。
- 与 `<limits.h>` 类似，用于整数；`<float.h>` 用于浮点。
- 在数值计算中，使用这些宏检查范围、精度，避免硬编码常量，提高可移植性和数值稳定性。
- C23 未对 `<float.h>` 引入新宏，主要浮点改进在其他特性（如 `<math.h>` 函数）。
