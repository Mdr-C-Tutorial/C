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
