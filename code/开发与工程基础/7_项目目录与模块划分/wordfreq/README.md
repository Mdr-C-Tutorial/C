# wordfreq

统计文本中出现次数最多的单词。本目录是教程「开发与工程基础」第 7 章结束时的版本 v0.1，
只有源码、样例数据和一个手工构建脚本；后面的章节会在这个版本上增加构建文件、测试和工具。

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

需要支持 C17 的 GCC。在本目录执行：

```sh
sh build_by_hand.sh
./build/wordfreq -n 3 tests/data/sample.txt
```

输出与 `tests/data/sample.expected` 相同：

```text
      4 the
      2 on
      2 sat
```

## 目录

```text
wordfreq/
├── include/wf/       公开头文件：token.h、counter.h、report.h
├── src/              三个模块的实现：token.c、counter.c、report.c
├── app/main.c        命令行程序：解析参数、读文件、调用三个模块
├── tests/data/       样例输入 sample.txt 与期望输出 sample.expected
├── build_by_hand.sh  手工构建脚本，产物放在 build/
├── README.md
└── .gitignore        忽略 build/
```

模块之间的依赖只从上层指向下层：`app/main.c` 使用全部三个模块，`report` 使用 `counter`
中的 `wf_entry`，`token` 与 `counter` 互不依赖。
