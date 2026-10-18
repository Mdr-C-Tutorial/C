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
