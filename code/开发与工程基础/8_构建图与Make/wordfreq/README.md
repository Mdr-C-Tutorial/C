# wordfreq

统计文本中出现次数最多的单词。本目录是教程「开发与工程基础」第 8 章结束时的版本 v0.1，
源码与第 7 章相同，新增了 `Makefile`；构建不再靠手写脚本，改哪个文件就重编哪个文件。

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

需要支持 C17 的 GCC、GNU Make（Windows 上是 `mingw32-make`）和 binutils 的 `ar`。
在 bash 类 shell（Windows 上为 MSYS2 UCRT64 或 Git Bash）中，于本目录执行：

```sh
make
make test
./build/wordfreq -n 3 tests/data/sample.txt
```

`make` 把 `src/` 下的三个模块编译成 `build/*.o`，打包成静态库 `build/libwf.a`，再与
`app/main.c` 的目标文件链接成 `build/wordfreq`。

`make test` 用样例数据运行程序，并与 `tests/data/sample.expected` 比较；一致时打印
`sample: ok`，不一致时 `diff` 以非 0 状态结束，`make` 也随之失败。

| 命令 | 作用 |
| --- | --- |
| `make`（即 `make all`） | 构建 `build/wordfreq` |
| `make test` | 运行端到端比较 |
| `make run` | 用样例数据运行 |
| `make clean` | 删除 `build/` |

头文件依赖由 `-MMD -MP` 在编译时记录到 `build/*.d`，由文件末尾的 `-include` 读入。
修改 `include/wf/` 下的头文件后，`make` 会重编包含它的目标文件。第 7 章的
`build_by_hand.sh` 保留作对照，它每次都重编全部文件。

Windows 上还有一个差别：链接器生成的是 `build/wordfreq.exe`，而规则里的目标写的是
`build/wordfreq`，make 找不到这个文件，于是每次都重新链接一遍。程序照常运行
（bash 会自动补上 `.exe` 后缀）。第 16 章讲怎样按平台区分构建文件。

## 目录

```text
wordfreq/
├── include/wf/       公开头文件：token.h、counter.h、report.h
├── src/              三个模块的实现：token.c、counter.c、report.c
├── app/main.c        命令行程序：解析参数、读文件、调用三个模块
├── tests/data/       样例输入 sample.txt 与期望输出 sample.expected
├── Makefile          构建图：目标、先决条件与配方
├── build_by_hand.sh  第 7 章的手工构建脚本，保留作对照
├── README.md
└── .gitignore        忽略 build/
```

模块之间的依赖只从上层指向下层：`app/main.c` 使用全部三个模块，`report` 使用 `counter`
中的 `wf_entry`，`token` 与 `counter` 互不依赖。
