# `<inttypes.h>`

`<inttypes.h>` 是 C99 引入的标准头文件，在 C23（ISO/IEC 9899:2024）中核心内容未变。它包含`<stdint.h>`的所有定义，并额外提供用于`printf`、`scanf`家族函数的格式化宏。这些宏用于正确打印和扫描`<stdint.h>`中定义的固定宽度整数类型（如`int32_t`、`uint64_t`等），因为这些类型的实际底层类型（如`long`或`long long`）在不同平台上可能不同。

使用方式：

```c
#include <inttypes.h>
#include <stdio.h>  // 或 <wchar.h> 用于宽字符版本
```

## 格式化宏分类

所有宏均为字符串字面量，扩展为适合平台的格式说明符（如`"d"`、`"ld"`、`"lld"`等）。宏名称遵循统一模式：

1. **打印宏（printf 系列）**：以 `PRI` 开头
   - `PRIdN` / `PRIiN`：有符号十进制（d 或 i）
   - `PRIoN`：无符号八进制（o）
   - `PRIuN`：无符号十进制（u）
   - `PRIxN`：无符号十六进制小写（x）
   - `PRIXN`：无符号十六进制大写（X）

   N 为位宽：8、16、32、64、LEAST8、LEAST16 等，或 `MAX`（对应`intmax_t`）、`PTR`（对应`intptr_t`）。

2. **扫描宏（scanf 系列）**：以 `SCN` 开头
   - `SCNdN` / `SCNiN`：有符号十进制
   - `SCNoN`：无符号八进制
   - `SCNuN`：无符号十进制
   - `SCNxN` / `SCNXN`：无符号十六进制（小写/大写）

   N 同上。

示例宏（实际扩展取决于实现）：

- `PRId32` → "d"（32 位系统上可能为 "ld"）
- `PRIu64` → "llu"
- `PRIdMAX` → "jd"（对应`intmax_t`）
- `SCNxPTR` → "lx" 或 "llx"

## 使用示例

```c
#include <inttypes.h>
#include <stdio.h>

int main() {
    int32_t  a = -123456;
    uint64_t b = 18446744073709551615ULL;

    // printf 使用打印宏
    printf("有符号 32 位：%" PRId32 "\n", a);
    printf("无符号 64 位：%" PRIu64 "\n", b);

    // scanf 使用扫描宏
    int64_t c;
    printf("输入有符号 32 位整数：");
    scanf("%" SCNd64, &c);  // 读取有符号 64 位整数

    return 0;
}
```

可能的输出：

<TerminalWindow title="zsh - test_inttypes">

有符号 32 位：-123456

无符号 64 位：18446744073709551615

输入有符号 32 位整数：**-2147483648**

</TerminalWindow>

## 宽字符版本（fwprintf、fwscanf）

宏前缀加 `PRI`/`SCN` 后接宽度修饰（如 `le`、`la`），但主要用于宽类型：

- 示例：`PRIxMAX` 用于`intmax_t` 的十六进制。

## 注意事项

- 宏必须与`%`连接使用：`%" PRId32`（中间无逗号）。
- 不使用这些宏会导致可移植性问题（如在 64 位系统上用`"lld"`打印`int64_t`，而在 32 位系统上出错）。
- `<inttypes.h>` 还定义`imaxdiv_t` 和 `imaxabs`、`imaxdiv` 函数（用于`intmax_t` 的绝对值和除法），但格式化宏是其最常用部分。
- 现代编译器（如 GCC、Clang、MSVC）均完整支持这些宏。

这些宏确保固定宽度整数在 I/O 操作中的平台独立性，是编写可移植 C 代码的必需工具。
