# `<stddef.h>`

**<stddef.h>** 定义了一些跨平台、跨实现的通用类型、宏和常量，几乎所有C程序都会直接或间接用到它们。

很多其他标准头文件（如`<stdlib.h>`、`<stdio.h>`、`<string.h>`等）都会隐式包含`<stddef.h>`，所以你经常不需要手动包含它。但当你直接使用其中的类型或宏时，最好显式包含，以提高代码可读性和可移植性。

## 如何使用

```c
#include <stddef.h>
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


## 1. size_t —— 表示对象大小的无符号整数类型

- **定义目的**：`sizeof` 操作符的返回类型，也是很多函数（如`malloc`、`strlen`）用来表示大小或长度的参数类型。
- **特点**：无符号整数，能表示任何对象可能的最大大小。
- **典型实现**：通常是 `unsigned long long`。
- **示例**：

  ```c
  #include <stddef.h>
  #include <stdio.h>

  int main() {
      size_t size = sizeof(int);  // 获取 int 类型占多少字节
      printf("int 的大小是：%zu 字节\n", size);  // 注意：打印 size_t 用 %zu
      return 0;
  }
  ```

  可能的输出：

  <TerminalWindow title="zsh - test_sizeof">

  int 的大小是：4 字节

  </TerminalWindow>

- **小贴士**：永远不要用 `int` 或 `unsigned int` 来代替`size_t`，这样能保证代码在不同平台上的可移植性。

## 2. ptrdiff_t —— 指针相减结果的带符号整数类型

- **定义目的**：两个指针相减的结果类型（例如数组中元素之间的距离）。
- **特点**：带符号整数，能表示同一数组内任意两个指针的差值（包括负值）。
- **典型实现**：通常是 `long` 或 `long long` 的有符号版本。
- **示例**：

  ```c
  #include <stddef.h>
  #include <stdio.h>

  int main() {
      int arr[] = {10, 20, 30, 40};
      int *p1 = &arr[0];
      int *p2 = &arr[3];
      ptrdiff_t diff = p2 - p1;  // 结果可能是 3
      printf("指针差：%td\n", diff);  // 打印 ptrdiff_t 用 %td
      return 0;
  }
  ```

  可能的输出：

  <TerminalWindow title="zsh - test_ptrdiff">

  指针差：3

  </TerminalWindow>

- **小贴士**：在循环遍历数组时，常用 `ptrdiff_t` 作为索引差来避免溢出问题。

## 3. wchar_t —— 宽字符类型

- **定义目的**：用于表示宽字符（wide character），支持更大的字符集（如Unicode）。
- **特点**：整数类型，足够大到能容纳任何支持的宽字符编码。
- **示例**：

  ```c
  #include <stddef.h>
  #include <wchar.h>  // 宽字符函数通常在这里

  int main() {
      wchar_t wc = L'中';  // 宽字符字面量用 L 前缀
      return 0;
  }
  ```

## 4. max_align_t —— 最大对齐要求的标量类型（C11及以后引入）

- **定义目的**：一个对象类型，它的対齐要求是所有标量类型中最大的。
- **用途**：在动态分配内存时，确保内存能满足最严格的对齐需求（比如用在 `aligned_alloc` 或自定义内存池）。
- **示例**：

  ```c
  #include <stddef.h>
  #include <stdlib.h>

  int main() {
      void *p = aligned_alloc(alignof(max_align_t), sizeof(max_align_t) * 10);
      // 分配的内存能满足最严格的对齐
      free(p);
      return 0;
  }
  ```

- 这个功能在编写需要严格对齐的代码（如操作系统内核或嵌入式）时特别有用。

## 5. NULL —— 空指针常量

- **定义**：通常是`((void*)0)` 或 `0`。
- **用途**：表示指针不指向任何有效对象。
- **示例**：

  ```c
  #include <stddef.h>

  int main() {
      int *p = NULL;  // 安全初始化指针
      if (p == NULL) {
          // 指针为空
      }
      return 0;
  }
  ```

- 注：C23 引入了 `nullptr`，在初始化指针的时候推荐写 `nulllptr`。

## 6. offsetof —— 计算结构体成员偏移的宏

- **定义**：`offsetof(type, member)` 返回成员`member`相对于结构体`type`起始地址的字节偏移量。
- **特点**：结果类型是`size_t`。
- **示例**：

  ```c
  #include <stddef.h>
  #include <stdio.h>

  struct Student {
      int id;
      char name[20];
      double score;
  };

  int main() {
      size_t offset = offsetof(struct Student, score);
      printf("score 成员的偏移量：%zu 字节\n", offset);
      return 0;
  }
  ```

  可能的输出：

  <TerminalWindow title="zsh - test_offsetof">

  score 成员的偏移量：24 字节

  </TerminalWindow>

- **小贴士**：常用于调试、序列化、内存布局分析等场景。注意：不能用于有柔性数组成员或位域的复杂情况。

## 总结

`<stddef.h>` 虽然内容不多，但它是C语言的“基石”之一——它提供的类型和宏保证了代码的**可移植性**和**安全性**。在实际编程中：

- 几乎所有涉及大小、指针、内存布局的代码都会用到它。
- 建议在需要这些类型时显式 `#include <stddef.h>`。
- 配合其他头文件（如 `<stdint.h>` 提供固定宽度整数，`<stdalign.h>` 提供对齐宏）使用，能写出更健壮的代码。
