# wordfreq

统计文本中出现次数最多的单词。本目录是教程「开发与工程基础」第 9 章结束时的版本 v0.1，
在第 8 章的 Makefile 之外新增了 `CMakeLists.txt` 与 `xmake.lua`；三份构建文件描述的是同一张
构建图，产物都在 `build/` 下。

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

### Make（第 8 章）

```sh
make
make test
./build/wordfreq -n 3 tests/data/sample.txt
```

### CMake

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/wordfreq -n 3 tests/data/sample.txt
```

`CMakeLists.txt` 定义两个目标：静态库 `wf`（`src/` 下三个模块）和可执行文件 `wordfreq`
（`app/main.c`）。`include/` 通过 `target_include_directories(wf PUBLIC include)` 传给
使用 `wf` 的目标，`wordfreq` 只需要 `target_link_libraries(wordfreq PRIVATE wf)`。

`CMAKE_EXPORT_COMPILE_COMMANDS ON` 让 CMake 在构建目录里写出 `compile_commands.json`，
clangd、clang-tidy 等工具按它找到每个文件的编译选项。

### XMake

```sh
xmake
xmake run wordfreq -n 3 tests/data/sample.txt
```

`xmake.lua` 用 `target("wf")` 与 `target("wordfreq")` 描述同一组目标，
`add_includedirs("include", {public = true})` 对应 CMake 的 PUBLIC 使用要求。

## 目录

```text
wordfreq/
├── include/wf/       公开头文件：token.h、counter.h、report.h
├── src/              三个模块的实现：token.c、counter.c、report.c
├── app/main.c        命令行程序：解析参数、读文件、调用三个模块
├── tests/data/       样例输入 sample.txt 与期望输出 sample.expected
├── Makefile          第 8 章的构建文件
├── CMakeLists.txt    CMake 的目标描述
├── xmake.lua         XMake 的目标描述
├── build_by_hand.sh  第 7 章的手工构建脚本，保留作对照
├── README.md
└── .gitignore        忽略 build/
```

模块之间的依赖只从上层指向下层：`app/main.c` 使用全部三个模块，`report` 使用 `counter`
中的 `wf_entry`，`token` 与 `counter` 互不依赖。
