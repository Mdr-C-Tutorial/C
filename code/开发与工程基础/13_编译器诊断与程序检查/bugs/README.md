# 第 13 章的注入缺陷与检查示例

本章的 `wordfreq` 在第 12 章基础上增加了 `WF_SANITIZE` 与 `WF_WERROR` 两个 CMake 选项，
警告选项改成 `-Wall -Wextra -Wshadow -Wformat=2 -Wmissing-prototypes -Wstrict-prototypes -Wvla`。
快照本身全部通过；下面的文件用来演示各类工具的报告。

## (a) dup_string 少分配一个字节

`counter_dup_off_by_one.c` 是 `src/counter.c` 的变体：`dup_string` 只 `malloc(len)`，
随后 `memcpy(copy, s, len + 1)` 写到缓冲区之外。

普通构建测试「通过」（多写的一个字节落在分配器的填充区里），ASan 构建报
`heap-buffer-overflow`。复现见 `bugs/README.md`。

## (b) load_file 在直线错误路径上忘了 fclose

`main_load_file_leak.c` 是一个独立的小程序：`load_file` 用
fopen、fseek/ftell、malloc、fread 读入整个文件，`fseek` 失败的分支直接 `return -1`，
没有 `fclose`。

```sh
gcc -std=c17 -Wall -Wextra -fanalyzer -c main_load_file_leak.c
```

GCC 13.2 与 14.2 都会给出 `-Wanalyzer-file-leak` 报告，带 (1)…(7) 的事件路径。
写成「返回 `char *`、`len` 用指针送出」的形式时，实测两版都不报，这就是分析器
既不健全也不完备的一个例子。

## UBSan 的两个小程序

- `ub_overflow.c`：`INT_MAX + argc`，实参来自运行时，编译器不能折叠；
- `ub_shift.c`：`1u << 40`，移位位数超出 `unsigned int` 的宽度。

两者都在 Linux（GCC 14.2）上实测，输出见下一节。

## (c) realloc 的返回值直接覆盖原指针

`realloc_uaf.c` 是 clang-tidy 一节用的例子：`c->items = realloc(c->items, ...)`，
失败时原缓冲区泄漏。`clang-tidy` 的 `bugprone-suspicious-realloc-usage` 会报出来。

## 修好的版本

`load_file_fixed.c` 是 (b) 用 goto 统一清理之后的版本，`-fanalyzer` 不再报告。
正文 3 节引用。

## 正文其他小程序

警告、诊断格式、ASan/LSan 的小程序在 `../demos/` 下：`warn.c`、`warn_more.c`、
`typo.c`、`missing_semicolon.c`、`zero_array.c`、`conversion.c`、`syntax.c`、
`dup_demo.c`、`uaf.c`、`leak.c`。

## 复现命令与真实输出

### (a) ASan 报告（Linux，GCC 14.2）

```sh
cp counter_dup_off_by_one.c ../wordfreq/src/counter.c
cd ../wordfreq
cmake -S . -B build-asan -DWF_SANITIZE=ON
cmake --build build-asan
./build-asan/test_counter
```

输出（节选，完整报告含 Shadow bytes）：

```text
==1198699==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000013
WRITE of size 4 at 0x502000000013 thread T0
    #0 0x7ff720af27ee in memcpy
    #1 ... in dup_string
    #2 ... in wf_counter_add
    #3 ... in test_add_and_get
0x502000000013 is located 0 bytes after 3-byte region [0x502000000010,0x502000000013)
allocated by thread T0 here:
    #0 ... in malloc
    #1 ... in dup_string
SUMMARY: AddressSanitizer: heap-buffer-overflow ... in memcpy
```

普通构建下同一个测试程序报告 `test_counter: 36 checks, 0 failed`，退出状态 0。

### (b) -fanalyzer 报告（Windows，GCC 13.2；Linux，GCC 14.2 相同）

```sh
gcc -std=c17 -Wall -Wextra -fanalyzer -c main_load_file_leak.c
```

```text
main_load_file_leak.c: In function 'load_file':
main_load_file_leak.c:30:16: warning: leak of FILE 'in' [CWE-775] [-Wanalyzer-file-leak]
   30 |         return -1;
      |                ^
  'load_file': events 1-7
    |
    |   23 |     FILE *in = fopen(path, "rb");
    |      |                ^~~~~~~~~~~~~~~~~
    |      |                |
    |      |                (1) opened here
    ...
    |      |                (7) 'in' leaks here; was opened at (1)
```

### UBSan（Linux，GCC 14.2）

```sh
gcc -std=c17 -Wall -Wextra -g -fsanitize=undefined -o ub_overflow ub_overflow.c
./ub_overflow
```

```text
ub_overflow.c:11:11: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
-2147483648 ./ub_overflow
```

默认报告后继续执行；加 `-fno-sanitize-recover=all` 后进程以非 0 状态终止。

```sh
gcc -std=c17 -Wall -Wextra -g -fsanitize=undefined -o ub_shift ub_shift.c
./ub_shift
```

```text
ub_shift.c:9:29: runtime error: shift exponent 40 is too large for 32-bit type 'unsigned int'
256
```
