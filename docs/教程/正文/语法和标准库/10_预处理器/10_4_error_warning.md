# `#error` 和 `#warning`

## `#error`

`#error` 指令用于在编译时显示指定的错误信息，使得程序 ill-formed，然后中止编译。

## `#warning`

`#warning` 指令用于在编译时显示指定的警告信息，不影响程序的有效性，继续编译。

## 示例

```c
#if __STDC__ != 1
#  error "Not a standard compiler"
#endif

#if __STDC_VERSION__ >= 202311L
#  warning "Using C23"
#endif

#include <stdio.h>

int main() {
    printf("Hello, world!\n");
}
```
