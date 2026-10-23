# 18.2.1 `<stddef.h>`

`<stddef.h>` 定义了一组通用的类型和宏：`size_t`、`ptrdiff_t`、`wchar_t`、`max_align_t`、`NULL` 和 `offsetof`。几乎所有 C 程序都会直接或间接用到它们。

`<stdio.h>`、`<stdlib.h>`、`<string.h>` 等头文件也定义了 `size_t`、`NULL` 等常用名称，所以只用到这些名称时，经常不需要手动包含 `<stddef.h>`。但使用 `ptrdiff_t`、`max_align_t`、`offsetof` 等名称时，应显式包含它：

```c
#include <stddef.h>
```

## 1. size_t

`size_t` 是 `sizeof` 运算符结果的类型，是一个无符号整数类型。`malloc`、`strlen` 等函数也用它表示大小或长度。它通常是 `unsigned long` 或 `unsigned long long`。

```c
#include <stddef.h>
#include <stdio.h>

int main(void) {
    size_t size = sizeof(int);  // 获取 int 类型占多少字节
    printf("int 的大小是：%zu 字节\n", size);  // 注意：打印 size_t 用 %zu
    return 0;
}
```

可能的输出：

::: terminal title="zsh - test_sizeof"

int 的大小是：4 字节

:::

表示对象大小和数组长度时，应使用 `size_t`，不要用 `int` 或 `unsigned int` 代替，这样代码在不同平台上都能正确工作。

## 2. ptrdiff_t

`ptrdiff_t` 是两个指针相减所得结果的类型，是一个带符号整数类型，通常是 `long` 或 `long long`。两个指向同一数组元素的指针相减，结果是二者下标之差，可以为负。

```c
#include <stddef.h>
#include <stdio.h>

int main(void) {
    int arr[] = {10, 20, 30, 40};
    int *p1 = &arr[0];
    int *p2 = &arr[3];
    ptrdiff_t diff = p2 - p1;  // 结果为 3
    printf("指针差：%td\n", diff);  // 打印 ptrdiff_t 用 %td
    return 0;
}
```

输出：

::: terminal title="zsh - test_ptrdiff"

指针差：3

:::

## 3. wchar_t

`wchar_t` 是表示宽字符 (wide character) 的整数类型，用于支持更大的字符集（如 Unicode），足以容纳实现所支持的任何宽字符编码。

```c
#include <stddef.h>
#include <wchar.h>  // 宽字符函数通常在这里

int main(void) {
    wchar_t wc = L'中';  // 宽字符字面量用 L 前缀
    (void)wc;
    return 0;
}
```

## 4. max_align_t

`max_align_t` 是 C11 引入的对象类型，它的对齐要求是所有标量类型中最大的。动态分配内存时，常用它保证内存满足最严格的对齐需求，例如用在 `aligned_alloc` 或自定义内存池中。

```c
#include <stdalign.h> /* 提供 alignof */
#include <stddef.h>
#include <stdlib.h> /* aligned_alloc 和 free 的完整讲解见第 11 章 */

int main(void) {
    void *p = aligned_alloc(alignof(max_align_t), sizeof(max_align_t) * 10);
    // 分配的内存能满足最严格的对齐
    free(p);
    return 0;
}
```

## 5. NULL

`NULL` 是空指针常量，通常定义为 `((void*)0)` 或 `0`，用于表示指针不指向任何对象。

```c
#include <stddef.h>

int main(void) {
    int *p = NULL;  // 安全初始化指针
    if (p == NULL) {
        // 指针为空
    }
    return 0;
}
```

注：C23 引入了 `nullptr`，在初始化指针的时候推荐写 `nullptr`。

## 6. offsetof

`offsetof(type, member)` 得到成员 `member` 相对于结构体 `type` 起始地址的字节偏移量，结果类型是 `size_t`。

```c
#include <stddef.h>
#include <stdio.h>

struct Student {
    int id;
    char name[20];
    double score;
};

int main(void) {
    size_t offset = offsetof(struct Student, score);
    printf("score 成员的偏移量：%zu 字节\n", offset);
    return 0;
}
```

可能的输出：

::: terminal title="zsh - test_offsetof"

score 成员的偏移量：24 字节

:::

`offsetof` 常用于调试、序列化和内存布局分析。

## 7. 习题

打印 `size_t` 和 `ptrdiff_t` 用 `%zu`、`%td`。本机 MinGW 的 GCC 格式检查不认这两个说明，按 11820 第 3 问的办法加 `-D__USE_MINGW_ANSI_STDIO=1`。7.2、7.3 的题目都要求 `gcc -std=c17 -Wall -Wextra` 零警告；7.1 的两道诊断题故意带着问题编译，按题面要求收集警告。

### 7.1 巩固

<Exercise id="11819" :d="2" :w="3">

下面的程序在查表失败时返回 `NULL`，但有两处把 `NULL` 用错了地方：

```c
#include <stddef.h>
#include <stdio.h>

struct entry {
    int key;
    const char *name;
};

static const struct entry table[] = {
    {1, "alpha"},
    {2, "beta"},
};

static const char *lookup(int key) {
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (table[i].key == key) {
            return table[i].name;
        }
    }
    return NULL;
}

int main(void) {
    const char *name = lookup(2);
    if (name = NULL) {
        printf("id 2 未找到\n");
    }
    printf("查到的名字：%s\n", name == NULL ? "(空)" : name);

    int missing = NULL;
    printf("missing = %d\n", missing);
    return 0;
}
```

1. 用 `gcc -std=c17 -Wall -Wextra` 编译，抄下全部诊断，指出每条对应哪一行。
2. `name = NULL` 是赋值还是比较？运行程序，输出是什么？这条语句执行后 `name` 指向哪里？把 `=` 改成 `==` 后重新运行，输出有什么变化？
3. `int missing = NULL;` 为什么能编译通过？编译器给出了什么诊断？`missing` 的值是多少？把它改成 `int missing = 0;`，确认诊断消失。
4. 在程序中加一行 `printf("%d\n", NULL);`，重新编译，抄下诊断，说明 `%d` 与 `NULL` 的类型为什么不匹配，应该改用哪个转换说明。
5. 改用 `-std=c2x` 编译，把程序里的 `NULL` 全部换成 `nullptr`。`if (name = nullptr)` 还会报警告吗？`int missing = nullptr;` 能通过编译吗？结合 [18.3](/教程/正文/语法和标准库/18_标准库的使用契约/18_3_功能特性测试宏) 说明 `nullptr` 与 `NULL` 在类型上的区别。

改对后的程序在 `lookup(2)` 时应打印 `查到的名字：beta`，不再出现「未找到」那一行；把调用改成 `lookup(9)`（顺便把那句提示里的 `id 2` 改成 `id 9`），应先打印 `id 9 未找到`，再打印 `查到的名字：(空)`。两种情况末尾都会输出 `missing = 0`。

</Exercise>

<Exercise id="11820" :d="3" :w="3">

下面的程序用 `size_t` 保存数组长度、用 `ptrdiff_t` 保存指针差，但格式说明和循环写法都有问题：

```c
#include <stddef.h>
#include <stdio.h>

int main(void) {
    int a[] = {10, 20, 30, 40};
    size_t count = sizeof a / sizeof a[0];
    ptrdiff_t back = &a[0] - &a[3];

    printf("count = %d\n", count);
    printf("back = %u\n", back);

    int n = sizeof a / sizeof a[0];
    printf("n = %d\n", n);

    for (size_t i = count - 1; i >= 0; i--) {
        printf("%d ", a[i]);
    }
    printf("\n");
    return 0;
}
```

1. 用 `gcc -std=c17 -Wall -Wextra` 编译，抄下三类诊断，写明每条涉及的实际类型。再分别只用 `-Wall`、只用 `-Wextra` 编译，指出每条诊断由哪个选项打开：格式相关的两条属于 `-Wformat`，属于 `-Wall`；循环的一条属于 `-Wtype-limits`，属于 `-Wextra`。这解释了只加 `-Wall` 时为什么会漏掉最危险的那条。
2. 运行程序。循环条件恒为真，程序不会正常结束，运行后按 Ctrl+C，或把标准输出重定向到文件再查看前几行。`back` 打印 `4294967293` 而不是 `-3`，从 `%u` 读取的位数和 `ptrdiff_t` 在这台机器上的实际类型解释这个结果。`count` 用 `%d` 打印也得到 4，编译器却为它报了格式诊断；`int n = sizeof a / sizeof a[0]` 同样丢掉了 `size_t` 的身份，`-Wall -Wextra` 却一个字不说。说明这两处各属于哪一类问题，以及本机上它们为什么都没让结果出错、换个平台或换个数组大小后哪一个会先出问题。
3. 把三处格式说明改成正确的，并把 `int n` 换成 `size_t`。本机 MinGW 上 GCC 的格式检查不认 `%zu` 和 `%td`，会报 `unknown conversion type character 'z'`；按 [3. 从源文件到可执行文件](/教程/正文/开发与工程基础/3_从源文件到可执行文件) 里的做法加上 `-D__USE_MINGW_ANSI_STDIO=1`，让 MinGW 换成自己的格式化实现，诊断随之消失。改完后前四行应输出什么？
4. 倒序循环有两种不越界的写法：

   ```c
   for (size_t i = count; i-- > 0;) {
       printf("%d ", a[i]);
   }

   for (ptrdiff_t i = (ptrdiff_t)count - 1; i >= 0; i--) {
       printf("%d ", a[i]);
   }
   ```

   第一种的判断在自减之后进行，第二种把长度先转换成有符号类型。分别说明它们为什么不会越界，并用 `-Wall -Wextra -Wconversion` 编译确认零警告。
5. 把 `&a[0] - &a[3]` 依次改成 `&a[3] - &a[0]` 和 `&a[0] - &a[0]`，记录 `%td` 的输出。指针相减的结果为什么必须是有符号类型？如果用 `size_t` 保存 `-3`，会得到什么？
6. 写一个 `static void show(int n)`，在 `main` 里调用 `show(count)`。只用 `-Wall -Wextra` 编译有诊断吗？加上 `-Wconversion` 呢？说明默认选项为什么不报，以及在本机把 64 位的 `size_t` 传进 32 位的 `int` 形参会截掉哪些位。

全部改完后，程序应输出：

```text
count = 4
back = -3
n = 4
40 30 20 10
```

</Exercise>

### 7.2 编程

<Exercise id="11821" :d="3" :w="3">

写一个探针程序 `wchar_probe.c`，把本机 `wchar_t` 的宽度和宽字符串的存储方式打印出来。被测字符串写成 `L"A中\U0001F600"`，同样内容的窄字符串写成 `"A中\U0001F600"`，分别取 `sizeof`、长度和每个元素的十六进制值。本机（MinGW-w64 GCC 13.2，`wchar_t` 为 16 位）的输出应为：

```text
sizeof(wchar_t) = 2
sizeof(L'A') = 2
sizeof(L"中文") = 6
wide: sizeof=10 wcslen=4
0041 4e2d d83d de00
narrow: sizeof=9 strlen=8
sizeof("中文") = 7
```

`wcslen` 返回以 `L'\0'` 结尾的宽字符串的代码单元数，声明在 `<wchar.h>` 里；窄字符串的长度用 `<string.h>` 的 `strlen`。

1. 写出程序并运行，与上面的输出对照。`sizeof(L"中文")` 为什么是 6，而不是 2 或 8？
2. `sizeof("中文")` 是 7，`sizeof(L"中文")` 是 6。两个字符串字面量写着同样的两个汉字，宽字符串反而占的字节更少。分别说明两个数是怎么算出来的，窄字符串和宽字符串各按什么编码存放，各自由哪几部分组成。
3. `wide` 那行有三个字符，`wcslen` 却返回 4。哪两个单元属于同一个字符？U+1F600 在这台机器上占几个 `wchar_t`？如果 `wchar_t` 是 32 位，`sizeof(L"中文")` 和这个被测宽字符串的 `wcslen` 各会是多少？
4. 不用 `wcslen`，自己写一个循环数到 `L'\0'`，确认结果一致。
5. 写 `wchar_t wc = L'\U0001F600';` 并打印它的十六进制值。编译器给出什么警告？存进 `wc` 的是哪个码元？由此说明一个 `wchar_t` 在这台机器上能否表示 U+1F600。

</Exercise>

<Exercise id="11822" :d="4" :w="4">

写一个按成员描述结构体内存布局的小工具。被测结构体为：

```c
struct sample {
    unsigned char tag;
    unsigned int id;
    unsigned short size;
    double value;
};
```

要求实现下面这些接口：

```c
#include <stdbool.h>
#include <stddef.h>

struct field_desc {
    const char *name;
    size_t offset;
    size_t size;
};

#define FIELD(type, member) \
    { #member, offsetof(type, member), sizeof(((type *)0)->member) }
#define FIELD_END \
    { NULL, 0, 0 }

bool layout_check(const struct field_desc *fields, size_t object_size, size_t *padding_out);
void dump_object(const void *obj, size_t object_size, const struct field_desc *fields);
```

1. `layout_check` 按顺序检查每个成员的偏移不重叠、不越出对象范围，统计成员之间以及末尾的填充字节总数；`padding_out` 返回这个总数，布局非法时返回 `false`。
2. `dump_object` 从对象起始地址逐字节打印，被成员覆盖的字节标出成员名，其余字节标成 `padding`。
3. 在 `main` 里把对象先用 `memset(&s, 0xAA, sizeof s)` 填满，再写入 `tag = 0x11`、`id = 0x22334455u`、`size = 0x6677`、`value = 1.5`，然后调用这两个函数。本机 x86-64 上应输出：

   ```text
   sizeof=24 padding=9 ok=1
   00: 11  tag
   01: aa  padding
   02: aa  padding
   03: aa  padding
   04: 55  id
   05: 44  id
   06: 33  id
   07: 22  id
   08: 77  size
   09: 66  size
   10: aa  padding
   11: aa  padding
   12: aa  padding
   13: aa  padding
   14: aa  padding
   15: aa  padding
   16: 00  value
   17: 00  value
   18: 00  value
   19: 00  value
   20: 00  value
   21: 00  value
   22: f8  value
   23: 3f  value
   ```

4. `padding=9` 由哪几段组成？成员大小之和是 15，`sizeof` 为什么是 24？从 `sizeof`、最后一个成员的偏移和结构体的对齐要求三者的关系解释。
5. 换一组嵌套成员，验证 `offsetof` 的第二个参数可以一路写下去：

   ```c
   struct point {
       int x;
       int y;
   };

   struct shape {
       char kind;
       struct point origin;
       int sizes[4];
       char tag;
   };
   ```

   打印 `offsetof(struct shape, origin)`、`offsetof(struct shape, origin.y)`、`offsetof(struct shape, sizes[2])`、`sizeof(struct shape)`，本机应依次得到 `4 8 20 32`。再用指针相减 `(char *)&s.origin.y - (char *)&s` 核对第二个数。后两个偏移与 `origin` 的偏移是什么关系？数组下标的写法在这里按什么单位计算？`tag` 的偏移是 28，为什么 `sizeof` 是 32 而不是 29？
6. 对位域和拼错的成员名各试一次：给出 `struct bf { unsigned a : 3; unsigned b : 5; };`，对 `b` 使用 `offsetof`；再写 `offsetof(struct shape, nope)`。分别抄下编译错误，说明 `offsetof` 为什么不能用于位域。
7. `sizeof(((struct sample *)0)->value)` 在空指针上取成员，为什么不会出错？`sizeof` 的操作数会被求值吗？

</Exercise>

### 7.3 综合与挑战

<Exercise id="11823" :d="5" :w="4">

实现一个定长分配区，把一块事先取得的存储切成若干大小不一的对象，每个对象的起始地址都要满足最严格的对齐要求。接口如下：

```c
#include <stddef.h>

struct arena {
    unsigned char *base;
    size_t capacity;
    size_t used;
};

void arena_init(struct arena *a, void *buffer, size_t capacity);
void *arena_alloc(struct arena *a, size_t size);
void arena_reset(struct arena *a);
size_t arena_used(const struct arena *a);
```

1. `arena_alloc` 先把 `used` 向上取整到 `alignof(max_align_t)` 的倍数，再分配 `size` 字节。`size` 为 0 时返回 `NULL`，不改动 `used`；剩余空间不足时返回 `NULL`，同样不改动 `used`。
2. 用 `malloc(4096)` 取得缓冲区，依次分配 1、2、3、8、17、33、64 字节，打印每块相对缓冲区的偏移和 `used`。本机应得到：

   ```text
   i=0 size=1 off=0
   i=1 size=2 off=16
   i=2 size=3 off=32
   i=3 size=8 off=48
   i=4 size=17 off=64
   i=5 size=33 off=96
   i=6 size=64 off=144
   intact=1 used=208
   too_big_null=1 used=208
   ```

3. 每块分配后写入一个互不相同的字节，例如第 i 块填 `i + 1`，全部分配完再逐块核对。这一步验证什么？如果取整写错、相邻两块重叠，会先在哪块上暴露出来？
4. 把向上取整的模数从 `alignof(max_align_t)` 换成 `sizeof(max_align_t)`，重新运行。本机 `used` 从 208 变成 288。这 80 个字节花在哪里？两种做法都能让每一块满足 `max_align_t` 的对齐要求吗？为什么其中一种更浪费？
5. 另写一个 `arena_reset` 之后分配整块 4096 字节的测试，再分配 1 字节，确认第二次返回 `NULL`。`used` 在成功与失败两种情况下各是多少？
6. 去掉缓冲区的对齐保证，改用普通静态数组 `static unsigned char pool[4096];`，再写一个 `struct holder { char c; unsigned char buf[64]; };`，打印 `alignof(unsigned char[64])`、`offsetof(struct holder, buf)`、`alignof(struct holder)` 和 `alignof(pool)`。本机这四个值都是 `1`；把 `pool` 改成 `static alignas(max_align_t) unsigned char pool[4096];`（`<stdalign.h>`）后，最后一个变成 `16`。说明 `alignof` 对数组给出的是元素类型的对齐，为什么它不足以说明“这块缓冲区能存放任何类型”；也说明为什么正文的分配区要让调用方保证缓冲区对齐，而不是自己去检查。
7. 正文用 `aligned_alloc` 演示 `max_align_t`。本机 MinGW-w64 13.2 上 `<stdlib.h>` 没有声明 `aligned_alloc`，也没有对应的符号，照抄会同时得到隐式声明警告和链接错误。先实测这一点，然后说明：需要一块严格对齐的存储时，分配区和 `aligned_alloc` 各自适合什么场合？分配区这种写法在独立实现（没有 `malloc`）里还能用吗？

</Exercise>

<Exercise id="11824" :d="6" :w="4">

`offsetof` 最有名的用法是用成员地址反推外层对象的地址。内核和各种容器库把它写成 `container_of`。已知链表节点和两种宿主记录：

```c
struct list_node {
    struct list_node *next;
    int kind;
};

struct sensor {
    int id;
    struct list_node node;
    double value;
};

struct actuator {
    char name[12];
    struct list_node node;
    int state;
};
```

`kind` 取 1 表示 `sensor`，取 2 表示 `actuator`。

1. 写出 `container_of(ptr, type, member)` 宏，要求先把成员指针转成 `char *`，减去 `offsetof` 之后再转回 `type *`。说明这两个转换各自解决什么问题。
2. 建立一条链表，把一个 `sensor` 和一个 `actuator` 通过各自的 `node` 串起来，写 `dump_list` 从表头出发遍历。每到一个节点，按 `kind` 决定外层类型，用 `container_of` 取回宿主对象并打印字段。本机 x86-64 上应输出：

   ```text
   sensor id=7 value=21.5 offsetof(node)=8
   actuator name=valve state=1 offsetof(node)=16
   container_of(sensor) ok=1
   container_of(actuator) ok=1
   sizes: node=16 sensor=32 actuator=40
   ```

3. 把宏写成错误版本：

   ```c
   #define bad_container_of(ptr, type, member) \
       ((type *)(ptr) - offsetof(type, member))
   ```

   对两个对象分别计算错误版本得到的指针与真实对象地址之差，按字节打印。本机应得到 `-248` 和 `-624`。用 `offsetof` 的值和 `sizeof(type)` 算出这两个数，说明指针减法为什么按整个结构体的大小缩放，而不是按字节。
4. 宏里 `(char *)(ptr) - offsetof(...)` 和 `(type *)((char *)(ptr) - offsetof(...))` 两步为什么不能合并成一步指针减法？如果 `ptr` 的声明类型恰好是 `void *`，能直接做算术吗？
5. 增加第三种记录：

   ```c
   struct reading {
       unsigned short channel;
       struct list_node node;
       int raw;
   };
   ```

   链表遍历和 `container_of` 的代码保持不变，只在按 `kind` 分派的地方加一个分支。本机应能接着打印 `reading ch=3 raw=-42 offsetof(node)=8`。说明为什么链表节点不需要知道宿主类型，也不需要为每种记录写一套链表函数。
6. 用 `offsetof` 与 `sizeof` 打印三种宿主结构的布局，并解释 `sensor` 的 `node` 偏移为 8、`actuator` 的 `node` 偏移为 16 分别由哪个成员的宽度和对齐要求决定。如果把 `actuator` 的 `char name[12]` 改成 `char name[9]`，偏移会变吗？

</Exercise>
