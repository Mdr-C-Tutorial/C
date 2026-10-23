# 第 3 章 从源文件到可执行文件

正文见 `docs/教程/正文/开发与工程基础/3_从源文件到可执行文件.md`。

| 文件 | 用途 |
| --- | --- |
| `hello.c` | 四阶段演示：注释与宏在预处理中的变化、`printf` 被换成 `puts` |
| `macro_demo.c` | `-E`、`-DDEBUG` 与条件编译 |
| `area_main.c`、`area.c` | 只声明不定义导致的链接错误，以及补上定义后的结果 |
| `root.c` | Linux 上 `sqrt` 与 `-lm`，`-l` 的位置 |
| `const.c` | 常量实参的 `sqrt`/`pow` 被编译器折叠，不需要 `-lm` |
| `calc.c` | `-O0` 与 `-O2` 的 `size` 与汇编长度对比 |
| `symbols.c` | `nm` 的符号类型 |
| `impl.c` | GCC 14 与 GCC 13 对未声明函数的不同处理 |
| `exercises/` | 习题配套，见其中的 README |

构建方式：每个文件都是单文件程序，例如

```sh
gcc -std=c17 -Wall -Wextra -o hello hello.c
```

`root.c` 在 Linux 上需要链接数学库（`-lm`）；`const.c` 的实参是常量，编译器会直接算出结果，不需要。
