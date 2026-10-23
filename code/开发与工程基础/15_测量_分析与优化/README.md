# 第 15 章的测量示例

本目录不属于 wordfreq 项目，是本章正文单独引用的两个小程序。

| 文件 | 用途 | 构建 |
| --- | --- | --- |
| `bench_strlen.c` | 第 7 节：比较「结果没人用」与「结果写进 volatile 对象」两种微基准写法 | `gcc -std=c17 -Wall -Wextra -O2 bench_strlen.c -o bench_strlen` |
| `gprof_dll/loop.c` | 第 4 节：演示 gprof 统计不到动态库内的时间 | `gcc -std=c17 -O2 -pg -no-pie loop.c -o loop` |

`bench_strlen` 需要一段文本作参数：

```sh
./bench_strlen "the quick brown fox jumps over the lazy dog and keeps on running"
```

在 MinGW-w64 GCC 13.2 上，`-O2` 下第一行必然是 0.000 秒，第二行才是真正的时间。
`gprof_dll/loop` 的墙钟时间约 0.2 秒，gprof 多数时候报告 `no time accumulated`。

wordfreq 的代码、输入生成器和基准脚本在上一级目录 `wordfreq/` 里。