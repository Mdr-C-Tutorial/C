# 17.5 `#embed` 包含二进制资源

`#embed` 可以在编译时包含（一般是二进制的）文件的内容，生成一个整数常量的列表。

## 1. 用法

基本语法：

```c
#embed "文件路径" [可选参数]
```

可选参数：

- `prefix(...)` 和 `suffix(...)`：在资源展开结果前后插入记号；
- `limit(...)`：限制读取的最大字节数；
- `if_empty(...)`：资源为空时使用替代记号。

示例 1：嵌入文件内容到数组

```c
const unsigned char image_data[] = {
    #embed "image.png"
};
```

示例 2：带限制和空文件处理

```c
const char config_data[] = {
    #embed "config.bin" limit(1024) if_empty("default")
};
```

## 2. `__has_embed`

`__has_embed` 用于在 `#if` 中检测能否找到某个资源，以及资源是否为空：

```c
#if __has_embed("文件路径")
    // 找到了资源（可能为空）
#else
    // 未找到资源
#endif
```

结果是下列三个预定义宏之一：

- `__STDC_EMBED_FOUND__`：找到非空资源；
- `__STDC_EMBED_EMPTY__`：找到空资源；
- `__STDC_EMBED_NOT_FOUND__`：未找到资源。

`#embed` 与 `__has_embed` 属于 C23 新增能力，旧标准或旧工具链上可能不可用。可以用 `#ifdef __has_embed` 判断编译器是否支持它们；写可移植代码时，建议始终保留回退路径。

## 3. 完整示例

```c
#include <stdio.h>

int main(void) {
#if __has_embed("data.bin") == __STDC_EMBED_FOUND__
    static const unsigned char data[] = {
#embed "data.bin"
    };
    printf("Loaded %zu bytes\n", sizeof data);
#elif __has_embed("data.bin") == __STDC_EMBED_EMPTY__
    puts("Loaded 0 bytes");
#else
    puts("data.bin was not found");
#endif
    return 0;
}
```

结果由构建时的 `data.bin` 决定：

::: code-group

```text [3 字节资源]
Loaded 3 bytes
```

```text [空资源]
Loaded 0 bytes
```

```text [资源不存在]
data.bin was not found
```

:::

## 4. 习题

这一页的习题都要用资源文件，先写一个 `mkdata.c` 把文件备齐（本机 GCC 编译运行即可）：

```c
#include <stdio.h>

static int write_file(const char *path, const unsigned char *data, size_t n) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }
    if (n != 0 && fwrite(data, 1, n, fp) != n) {
        perror(path);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

int main(void) {
    static const unsigned char blob[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    if (write_file("blob.bin", blob, sizeof blob) != 0) {
        return 1;
    }
    if (write_file("payload.bin", blob, sizeof blob) != 0) {
        return 1;
    }
    if (write_file("empty.bin", NULL, 0) != 0) {
        return 1;
    }
    if (write_file("greeting.txt", (const unsigned char *)"hello", 5) != 0) {
        return 1;
    }
    return 0;
}
```

`gcc -std=c17 -Wall -Wextra mkdata.c -o mkdata` 后运行，当前目录就有 `blob.bin`、`payload.bin`（4 字节）、`empty.bin`（0 字节）与 `greeting.txt`（5 字节）。`#embed` 要求编译器支持 C23；本机 GCC 13.2 不认，下面的题目用 Clang 19 加 `-std=c23` 编译。

<Exercise id="11718" :d="2" :w="2">

阅读程序，先不编译，写出下面的三个分支各打印哪一行：

```c
#include <stdio.h>

int main(void) {
#if __has_embed("blob.bin")
    static const unsigned char blob[] = {
#embed "blob.bin"
    };
    printf("blob: %d bytes\n", (int)sizeof blob);
#else
    puts("blob: none");
#endif

#if __has_embed("empty.bin")
    puts("empty: taken");
#else
    puts("empty: skipped");
#endif

#if __has_embed("missing.bin")
    puts("missing: taken");
#else
    puts("missing: skipped");
#endif
    return 0;
}
```

1. 写出运行结果。
2. `__STDC_EMBED_FOUND__`、`__STDC_EMBED_EMPTY__`、`__STDC_EMBED_NOT_FOUND__` 的值依次是 1、2、0。据此解释：空文件为什么也走进了 `#if` 的真分支。
3. 把第二个条件改成 `#if __has_embed("empty.bin") == __STDC_EMBED_FOUND__`，重新编译运行，哪一行变了？
4. 正文用 `#ifdef __has_embed` 判断编译器是否支持。如果只写这一层，程序还能区分「资源为空」与「资源不存在」吗？用一个实验说明：把 `#if __has_embed("blob.bin")` 换成 `#ifdef __has_embed` 后，再把 `blob.bin` 删掉，编译会怎样。

</Exercise>

<Exercise id="11719" :d="3" :w="3">

`greeting.txt` 里是 5 个字节的 `hello`，没有换行。把四个数组放进同一个程序，打印各自的 `sizeof`，再对 `terminated` 与 `first3` 打印 `strlen` 和字符串：

```c
static const char raw[] = {
#embed "greeting.txt"
};
static const char terminated[] = {
#embed "greeting.txt"
    , 0
};
static const char first3[] = {
#embed "greeting.txt" limit(3)
    , 0
};
static const char suffixed[] = {
#embed "greeting.txt" suffix(, 0)
};
```

1. 写出全部输出。
2. `raw` 能装下 5 个字符，却不能传给任何按 C 字符串工作的函数。说明原因；那个结尾的 0 是从哪来的？
3. `terminated` 与 `suffixed` 的字节完全一样，两种写法各适合什么场合？
4. 把 `first3` 的 `limit(3)` 改成 `limit(8)`。文件只有 5 字节，多出来的 3 个字节不会出现，也不会报错。改完之后 `sizeof first3` 与打印的字符串分别是什么？

</Exercise>

<Exercise id="11720" :d="3" :w="3">

`payload.bin` 里有 4 个字节 `DE AD BE EF`。下面的 `frame` 与 `tag` 各占多少字节，初始值分别是什么？

```c
static const unsigned char frame[] = {
#embed "payload.bin" limit(2) prefix(0xAA,) suffix(,0x55)
};

struct magic {
    unsigned char a, b, c, d;
};

static const struct magic tag = {
#embed "payload.bin"
};
```

1. 写出两个对象的字节数与每个字节的值。
2. 把 `prefix(0xAA,)` 改成 `prefix(0xAA)` 再编译，记录报错。对照 `suffix(,0x55)`，说明圆括号里的记号是怎样插进展开结果的，分隔用的逗号为什么得自己写。
3. 用前面那个 `mkdata.c` 另造一个 6 字节的文件与一个 2 字节的文件，分别替换 `tag` 里的资源。记录编译器对 6 字节那份的诊断；对 2 字节那份打印四个成员。多出来和缺掉的字节分别怎么处理？
4. 把 `frame` 换成嵌入空文件：

```c
static const unsigned char fallback[] = {
#embed "empty.bin" if_empty(0x00)
};
```

`fallback` 有几个字节？把资源名改成不存在的 `missing.bin`、仍保留 `if_empty(0x00)`，能编译通过吗？两次结果说明 `if_empty` 管的是什么、不管什么。

</Exercise>

<Exercise id="11721" :d="4" :w="4">

`#embed` 出现之前，把二进制资源放进程序要靠工具把字节转成 C 数组源码。这个练习把两条路放在一起比。

1. 写一个 `bin2c`：命令行接输入文件与输出文件两个参数，生成形如下面的源码，数组名固定为 `blob`，末尾再输出常量 `blob_size`：

```c
static const unsigned char blob[] = {
    0xDE, 0xAD, 0xBE, 0xEF,
};
static const unsigned long blob_size = 4UL;
```

2. 准备一份 1 MiB 的二进制文件（内容随意，Windows 下可用 `fsutil file createnew big.bin 1048576`，Linux 下 `head -c 1048576 /dev/urandom > big.bin`）。分别生成 `via_bin2c.c` 与 `via_embed.c`，两个文件都提供 `const unsigned char *resource_data(void)`，数组用 `static const`。记录生成的源文件多大、编译各用多少时间、目标文件多大。两份目标文件的大小应当接近，编译时间不会一样，说说时间差在哪一步。
3. 编译时加上 `-MMD`（GCC 与 Clang 都支持），编译器会另写一个与目标文件同名的 `.d` 文件，列出这次编译用到的全部文件。对两条路各做一次，看 `.d` 里有没有那个二进制文件；再把二进制文件改一个字节后重新构建，说明两条路在增量构建上的差别。这一步用到的选项属于构建系统的内容，不在本章范围内，照上面的说明操作即可。
4. `#embed` 只在 C23 下可用。给出一种「优先用 `#embed`、不支持时退回 `bin2c`」的组织方式，并说明生成出来的数组源码该不该进版本库。提示：能不能用 `#if defined(__has_embed)` 把两条路包在同一个文件里？

</Exercise>
