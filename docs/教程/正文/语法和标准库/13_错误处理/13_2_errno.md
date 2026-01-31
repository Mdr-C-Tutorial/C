# `<errno.h>`

`<errno.h>` 提供了宏 `errno` 以及一组错误码宏（如 `EDOM`、`ERANGE` 等）。它们用于让库函数在“失败时”给出更具体的原因。

在 C 语言里，常见的错误报告模式是：

1. 函数通过**返回值**表示成功/失败。
2. 当返回值表示失败时，再读取 `errno` 了解失败原因。

## 1. `errno` 是什么

`errno` 是一个可修改的左值 (modifiable lvalue)，其类型为 `int`；它通常被实现成宏。

当某些库函数发生错误时，它们会把 `errno` 设置为某个非零值（错误码）。但是：

1. **成功时不保证把 `errno` 置零**。
2. **不是所有函数都会设置 `errno`**。
3. 在支持多线程的实现中，`errno` 通常是线程局部存储 (Thread-local storage, TLS)。

因此，正确的用法是：先看函数的返回值，再决定是否读取 `errno`。

## 2. 典型用法模板

下面是一个通用模板：

```c
#include <errno.h>

errno = 0;
/* 调用可能失败的函数 */
if (/* 返回值表示失败 */) {
    /* 此时再读取 errno */
    int e = errno;
    /* 处理 e */
}
```

::: warning 先判断返回值，再看 errno

很多初学者直接检查 `errno != 0` 来判断失败，这是不可靠的，因为 `errno` 可能保留着上一次失败留下的值。

:::

## 3. 示例：用 `strtol` 做健壮的字符串转整数

`strtol` 的常见失败包括：

1. 输入不是数字（没有发生任何转换）。
2. 转换结果超出 `long` 的表示范围（范围错误）。

```c
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char* s = "9999999999999999999999";

    errno = 0;
    char* end = NULL;
    long x = strtol(s, &end, 10);

    if (end == s) {
        puts("no conversion");
        return 0;
    }

    if (errno == ERANGE && (x == LONG_MAX || x == LONG_MIN)) {
        puts("range error");
        return 0;
    }

    printf("%ld\n", x);
    return 0;
}
```

## 4. 示例：`fopen` 失败后用 `perror` 输出原因

`perror` 会根据当前 `errno` 输出一条人类可读的消息：

```c
#include <errno.h>
#include <stdio.h>

int main(void) {
    FILE* f = fopen("no_such_file.txt", "r");
    if (f == NULL) {
        perror("fopen");
        return 0;
    }

    fclose(f);
    return 0;
}
```

可能的输出：

<TerminalWindow>

fopen: No such file or directory

</TerminalWindow>

::: tip `errno` 与 I/O 的关系

并非所有 I/O 失败都只靠 `errno` 判断。很多 I/O 接口还需要结合返回值与流状态（如 `ferror`、`feof`）一起判断。

:::

## 5. 习题

1. 编写函数 `int parse_i32(const char* s, int* out);`，要求：
   1. 输入为十进制，允许前导空白与正负号。
   2. 当没有发生任何转换时返回错误。
   3. 当发生范围错误时返回错误，并能区分“超上界”和“超下界”。
2. 写一个小程序：读入一个文件路径，尝试打开并读取第一行；当打开失败时，使用 `perror` 输出原因。
3. 阅读并解释：为什么下面代码是不可靠的？给出修正版本。

```c
#include <errno.h>
#include <stdlib.h>

long f(const char* s) {
    char* end = NULL;
    long x = strtol(s, &end, 10);
    if (errno != 0) {
        return 0;
    }
    return x;
}
```
