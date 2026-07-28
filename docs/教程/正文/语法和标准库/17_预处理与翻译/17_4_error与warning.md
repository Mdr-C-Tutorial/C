# 17.4 `#error` 和 `#warning`

## `#error`

`#error` 指令用于在编译时显示指定的错误信息，使得程序 ill-formed，然后中止编译。

它常用于“前提检查”，例如编译器版本不满足、目标平台不在支持列表、关键宏缺失等。与运行时检查不同，`#error` 会把问题提前到构建阶段暴露出来。

## `#warning`

`#warning` 指令用于在编译时显示指定的警告信息，不影响程序的有效性，继续编译。

这里需要提一嘴：`#warning` 在 C23 才进入标准；更早版本里它通常是编译器扩展。若项目要求严格 C11/C17 兼容，可改用条件编译 + 注释提示，或使用构建系统输出警告。

## 适用场景

`#error` 适合“必须阻止构建继续”的约束，`#warning` 适合“可以继续，但希望显式提醒”的约束。二者都属于预处理阶段能力，不依赖程序运行路径。

## 示例

```c
#if __STDC__ != 1
#  error "Not a standard compiler"
#endif

#if __STDC_VERSION__ >= 202311L
#  warning "Using C23"
#endif

#include <stdio.h>

int main(void) {
    printf("Hello, world!\n");
    return 0;
}
```

在 C23 模式下，编译器必须为 `#warning` 发出诊断，但诊断文字由实现决定。如果编译继续并生成可执行文件，运行时输出为：

::: terminal

Hello, world!

:::
