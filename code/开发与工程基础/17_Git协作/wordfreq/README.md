# wordfreq

统计文本中出现次数最多的单词。本目录是教程「开发与工程基础」第 17 章结束时的版本 v0.2：
在第 16 章的构建配置之上增加了 `.gitattributes`（统一换行符）与 `tools/check.sh`
（`git bisect run` 用的检查脚本）。

## 用法

```text
usage: wordfreq [-n N] [FILE...]
```

1. 输出出现次数最多的 N 个单词，N 默认为 10，可取 1 到 1000000。
2. 没有 FILE 时读标准输入；给出多个 FILE 时把它们合并统计。
3. 单词是连续的 ASCII 字母（A–Z、a–z），统计时转成小写；其他字节（数字、标点、空白、
   0x80 以上的字节）都是分隔符。超过 63 个字母的单词只保留前 63 个。
4. 按次数从多到少输出，次数相同的按 `strcmp` 升序排列。每行先是右对齐、占 7 列的次数，
   再是一个空格和单词。
5. 退出状态：0 表示成功；1 表示文件无法打开或读取（标准错误输出
   `wordfreq: <路径>: <错误说明>`），或内存不足；2 表示用法错误（标准错误输出上面的用法行）。

## 构建与运行

### CMake 与 CTest

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

三个测试：

| 测试 | 类型 | 内容 |
| --- | --- | --- |
| `token` | 单元测试 | `wf_tokenizer_next` 的分词、标点、大小写、截断 |
| `counter` | 单元测试 | `wf_counter_add/get/size/top` 与同次数排序 |
| `e2e` | 端到端测试 | 运行 `wordfreq -n 3 tests/data/sample.txt`，与 `sample.expected` 比较 |

`ctest -L unit` 只运行带 `unit` 标签的单元测试，`ctest -R token` 按名字筛选。
端到端测试由 `tests/run_e2e.cmake` 执行，比较时忽略行尾的 `\r`（Windows 上标准输出是
文本模式）。

Sanitizer 构建放在单独的目录里，`WF_SANITIZE` 打开时编译和链接都加
`-fsanitize=address,undefined -fno-omit-frame-pointer`：

```sh
cmake -S . -B build-asan -DWF_SANITIZE=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

`-DWF_WERROR=ON` 把警告当错误处理，供 CI 与开发构建使用。在代码里制造一个少分配一个字节的
缺陷（`../13_编译器诊断与程序检查/bugs/counter_dup_off_by_one.c`），普通构建的测试照样通过，Sanitizer 构建才报出来。
MinGW 的 GCC 没有 ASan/UBSan 运行库，这两个选项只在 Linux 和 macOS 上能用。

### Make

```sh
make
make test
```

`make test` 依次运行两个单元测试程序，再用 `diff` 比较样例输出。

### 覆盖率与模糊测试

```sh
cmake -S . -B build-cov -DWF_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov
cd build-cov && gcov -b -o CMakeFiles/wf.dir/src CMakeFiles/wf.dir/src/token.c.gcno
```

`WF_COVERAGE` 给库和所有测试程序加 `--coverage -O0`，运行测试后在构建目录里生成
`.gcda`，`gcov` 据此写出 `token.c.gcov`。

```sh
cmake -S . -B build -DWF_FUZZ=ON
cmake --build build
ctest --test-dir build -R replay --output-on-failure
```

`WF_FUZZ` 构建 `fuzz/replay_main.c` 与 `fuzz/fuzz_token.c` 组合成的 `replay_token`，
把 `tests/regress/` 下的文件逐个送进 `LLVMFuzzerTestOneInput`，注册成 ctest 测试 `replay`。
libFuzzer 目标本身要用 clang 构建：

```sh
clang -g -O1 -fsanitize=fuzzer,address -Iinclude fuzz/fuzz_token.c src/token.c -o fuzz_token
./fuzz_token -max_total_time=30 corpus/
```

### XMake

`xmake.lua` 与第 9 章相同，测试与模糊测试的接入方式只作说明。

### 测量

```sh
cc -std=c17 -O2 -g -Iinclude tools/gen_words.c -o build/gen_words
build/gen_words 1000 1000000 42 > build/words-1000000.txt
sh tools/bench.sh
```

`tools/gen_words` 用固定种子的线性同余生成器造 M 个人工单词，再按偏斜分布抽取 N 个，
同样的参数每次都得到同样的文件。`tools/bench.sh` 用 `-O2 -g` 构建 v0.2，对
10^4、10^5、10^6 三种规模各跑 5 次，取 real 时间的中位数。它只适合在本机做粗略对照，
不要在共享服务器上运行。

用 gprof 找热点时 MinGW 需要 `-no-pie`，否则平面剖析为空：

```sh
gcc -std=c17 -O2 -g -pg -no-pie -Iinclude src/*.c app/main.c -o build/wordfreq_pg
build/wordfreq_pg -n 10 build/words-1000000.txt > /dev/null
gprof -b build/wordfreq_pg gmon.out
```

## 可移植构建

`include/wf/export.h` 定义 `WF_API`：静态库构建时展开为空，`BUILD_SHARED_LIBS=ON`
时给公开函数加上导出标记。

```sh
cmake -S . -B build-shared -DBUILD_SHARED_LIBS=ON
cmake --build build-shared
```

`app/main.c` 按 `include/wf/config.h` 里生成的 `WF_CONFIG_HAVE_GETLINE` 选择读文件的方式：
有 `getline` 时按行读入，否则用 `fgets` 分块读入，两条路径的行为相同。

```sh
cmake -S . -B build-nogetline -DWF_HAVE_GETLINE=OFF
```

`CMakePresets.json`（version 6）定义四个配置预设 `dev-gcc`、`release`、`asan`、`msvc`，
以及同名的构建、测试预设和一个 `ci` 工作流预设：

```sh
cmake --preset dev-gcc
cmake --build --preset dev-gcc
ctest --preset dev-gcc
cmake --workflow --preset ci
```

个人设置写进 `CMakeUserPresets.json`，不入库。

## 版本控制

`.gitattributes` 规定文本文件在索引里用 LF，`*.sh` 强制 LF，`*.bat` 用 CRLF，
图片按二进制处理。检出时不按平台转换，避免脚本在 Linux 上报 `$'\r'` 一类错误。

`tools/check.sh` 构建并运行全部测试，按退出状态报告结果，供 `git bisect run` 使用：

```sh
sh tools/check.sh
```

`../setup_demo.sh` 在工作目录内建一个裸仓库与两个克隆，用于本章的协作演示；
`../bisect_demo.sh` 生成 bisect 演示用的提交链。

## 目录

```text
wordfreq/
├── include/wf/       公开头文件：token.h、counter.h、report.h、export.h
├── src/              三个模块的实现：token.c、counter.c（v0.2）、report.c；config.h.in
├── app/main.c        命令行程序：解析参数、读文件、调用三个模块
├── tests/
│   ├── check.h       测试框架的接口：CHECK、CHECK_EQ_SIZE、CHECK_STR_EQ、check_report
│   ├── check.c       框架的实现
│   ├── test_token.c  分词模块的单元测试
│   ├── test_counter.c  计数模块的单元测试
│   ├── run_e2e.cmake 端到端测试脚本
│   ├── regress/      模糊测试的回归输入
│   └── data/         sample.txt 与 sample.expected
├── fuzz/
│   ├── fuzz_token.c  libFuzzer 目标
│   └── replay_main.c 重放程序，用 gcc 构建
├── tools/
│   ├── gen_words.c   可重复的输入生成器
│   ├── bench.sh      固定条件的基准脚本
│   └── check.sh      git bisect run 用的检查脚本
├── .gitattributes    换行符约定
├── CMakePresets.json 配置、构建、测试与工作流预设
├── Makefile          第 8 章的构建文件，test 目标已改为运行测试
├── CMakeLists.txt    CMake 的目标描述，enable_testing 与三个测试
├── xmake.lua         XMake 的目标描述
├── build_by_hand.sh  第 7 章的手工构建脚本，保留作对照
├── README.md
└── .gitignore        忽略 build/
```

模块之间的依赖只从上层指向下层：`app/main.c` 使用全部三个模块，`report` 使用 `counter`
中的 `wf_entry`，`token` 与 `counter` 互不依赖。
