# 第 14 章的注入缺陷与工具使用

本章的 `wordfreq` 在第 13 章基础上增加了 `fuzz/`、`tests/regress/` 与两个 CMake 选项
`WF_COVERAGE`、`WF_FUZZ`。

- `fuzz/fuzz_token.c`：libFuzzer 目标，第一个字节决定 `cap`，定义在 `fuzz_token.c`；
- `fuzz/replay_main.c`：重放程序，用 gcc 构建，把语料与崩溃输入当作 ctest 测试；
- `tests/regress/`：回归输入，含 `token_cap1_overflow.bin`（3 字节）与 `token_cap1_long_word.bin`（65 字节）。

## 注入缺陷：`cap` 为 1 的快路径漏掉截断检查

`bugs/token_cap1_overflow.c` 是 `src/token.c` 的变体：新增的 `if (cap == 1)` 分支
把单词原样复制进缓冲区，没有 `n + 1 < cap` 的检查，`word[n] = '\0'` 在单词长于
1 个字母时写到缓冲区之外。

单元测试全部通过：没有任何用例用 `cap` 为 1 调用 `wf_tokenizer_next`，gcov 报告里
这一整块是 `#####`。

```sh
cp bugs/token_cap1_overflow.c src/token.c
cmake --build build
ctest --test-dir build --output-on-failure
```

用 libFuzzer 与 ASan 构建（本机，VS 自带 clang 22）：

```sh
clang -g -O1 -fsanitize=fuzzer,address -Iinclude fuzz/fuzz_token.c src/token.c -o fuzz_token
./fuzz_token -max_total_time=30 corpus/
```

30 秒内找到崩溃（本机实测；下面是一次运行的真实输出，未编译过的读者可先用
`-O0` 复现，`-O1` 会把写入点归到函数末尾的 `}`）：

```text
==7448==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x1193d35a6371
WRITE of size 1 at 0x1193d35a6371 thread T0
    #0 0x7ff7a8e138cf in wf_tokenizer_next src/token.c:42
    #1 0x7ff7a8e1307a in LLVMFuzzerTestOneInput fuzz/fuzz_token.c:37
SUMMARY: AddressSanitizer: heap-buffer-overflow src/token.c:42 in wf_tokenizer_next
```

这一次崩溃的输入已经被压到 2 个字节，内容就是第一个字节 0（`cap = 1`）加一个字母。
`-minimize_crash=1` 对不同运行可能停在不同的最短输入上。放进
`tests/regress/token_cap1_overflow.bin` 的是 `00 00 62`（3 个字节）：第一个字节 0 使
`cap = 1`，后面的字母就写到缓冲区之外。它对修复前的代码必然触发，而且看得出原理，
所以用它做回归。

## 用重放程序做回归

```sh
cmake -S . -B build -DWF_FUZZ=ON
cmake --build build
ctest --test-dir build -R replay --output-on-failure
```

修复后的源码下 `replay` 通过，退出状态 0；把缺陷版本换回去，同一个输入让进程异常终止
（Windows 上退出状态 `0xC0000409`，Linux 上是 SIGABRT），ctest 报 `replay` 失败。
没有 clang 的 CI 也能靠这一步防止复发。

## 覆盖率

```sh
cmake -S . -B build-cov -DWF_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov
cd build-cov && gcov -b -o CMakeFiles/wf.dir/src CMakeFiles/wf.dir/src/token.c.gcno
```

未注入缺陷时（Linux，GCC 14.2）的真实输出：

```text
File '/home/minsecrus/tutorial/work/ch14/snap/wordfreq/src/token.c'
Lines executed:95.83% of 24
Branches executed:100.00% of 24
Taken at least once:87.50% of 24
Calls executed:100.00% of 3
Creating 'token.c.gcov'
```

`token.c.gcov` 里唯一标 `#####` 的行是 `if (cap == 0) return 0;` 的提前返回：
现有测试没有用 `cap == 0` 调用过 `wf_tokenizer_next`，补一个用例即可覆盖。

换上有缺陷的 `src/token.c` 之后，同一套测试仍然全部通过，而报告变成：

```text
Lines executed:75.00% of 32
Branches executed:86.67% of 30
Taken at least once:73.33% of 30
Calls executed:60.00% of 5
```

`if (cap == 1)` 下面的整块代码都是 `#####`——缺陷恰好躲在没有测试走到的地方。