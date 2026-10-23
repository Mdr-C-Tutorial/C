# messy_layout：反例项目（第 7 章习题 20705）

这是一个故意写乱的小项目，用来练习把它重构成 `include/`、`src/`、`app/` 分开的布局。
它目前能按下面这一条命令构建，结果正确：

```sh
gcc -std=c17 -Wall -Wextra main.c -o messy
./messy
```

```text
letters: 13
words: 2
```

毛病在别处：所有文件都堆在根目录；`globals.h` 里定义全局对象（每个包含它的翻译单元各有一份
定义）；`words.c` 用 `#include "util.c"` 引入实现；`main.c` 又 `#include "words.c"` 和
`#include "report.c"`。因此按常规方式把每个 `.c` 分别编译再链接，会得到一串
`multiple definition` 链接错误。

文件：

```text
messy_layout/
├── globals.h   两个全局计数对象（g_letter_count、g_word_count）
├── words.h     声明 count_letters 和 is_letter，并包含 globals.h
├── util.c      is_letter 的实现
├── words.c     count_letters 的实现，包含 util.c
├── report.c    print_report 的实现
└── main.c      包含 words.c 与 report.c，调用两个函数
```
