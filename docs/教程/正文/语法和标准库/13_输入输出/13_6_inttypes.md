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
