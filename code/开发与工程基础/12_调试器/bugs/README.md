# 第 12 章的两个注入缺陷

本章的 `wordfreq` 快照与第 11 章相同。两个缺陷都以「换掉某个源文件」的方式复现，
换回原文件即恢复。

## (a) 对齐输出 + main.c 传错项数，进程崩溃

缺陷由两个文件共同构成：

- `report_alignment.c`：`src/report.c` 的变体，按最长单词对齐输出，逐字符遍历
  `entries[i].word` 求宽度；
- `main_passes_n.c`：`app/main.c` 的变体，`print_top` 把请求的 `n` 而不是
  `wf_counter_top` 的返回值传给 `wf_report_print`。

`calloc` 分配了 `n` 个条目，实际只写入了 `min(n, size)` 个，其余条目的 `word` 是空指针，
report 的对齐循环解引用它。

```sh
cp bugs/report_alignment.c src/report.c
cp bugs/main_passes_n.c app/main.c
gcc -std=c17 -Wall -Wextra -g -O0 -D__USE_MINGW_ANSI_STDIO=1 \
    -Iinclude src/*.c app/main.c -o build/wordfreq_crash
./build/wordfreq_crash -n 10 tests/data/sample.txt
```

真实输出（Windows，MinGW-w64 GCC 13.2）：

```text
Segmentation fault
```

样例只有 3 个不同的单词，`-n 10` 时第 4 个条目的 `word` 为空指针，崩溃发生在
`src/report.c` 中求最长单词宽度的循环里。根因在调用方：应当传实际写入的项数
`wf_counter_top` 的返回值。

## (b) find_entry 前缀匹配，计数记到别的单词上

`counter_prefix_match.c` 是 `src/counter.c` 的变体：`find_entry` 写成
`strncmp(word, e->word, strlen(word)) == 0`。先加入 "then" 再加入 "the" 时，
"the" 会被当成 "then" 的一次出现。

```sh
cp bugs/counter_prefix_match.c src/counter.c
gcc -std=c17 -Wall -Wextra -g -O0 -D__USE_MINGW_ANSI_STDIO=1 \
    -Iinclude src/*.c app/main.c -o build/wordfreq_prefix
printf 'then the then the the\n' > build/prefix.txt
./build/wordfreq_prefix -n 5 build/prefix.txt
```

真实输出：

```text
      5 then
```

输入里 "then" 出现 2 次、"the" 出现 3 次，正确结果是两行；缺陷版本把它们并成了一项。
