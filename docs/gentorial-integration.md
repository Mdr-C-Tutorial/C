# GenTorial 接入

本教程接入 [GenTorial](https://gentorial.github.io/gentorial/) 框架。（这个框架也是笔者写的）

## C 标准版本不是编译器版本

::: concept c-standard-version title="语言标准与实现"
C11、C17、C23 描述的是 ISO C 语言标准版本；GCC、Clang 和 MSVC 是具体实现，不能把某个编译器支持的扩展直接当作 ISO C 能力。
:::

下面的程序只使用 C11 已具备的语法：

```c
#include <stdio.h>

int main(void) {
    puts("hello, Gentorial");
    return 0;
}
```

生成成功后，可以在结果末尾继续追问；从较早的回答节点提出新问题时，运行时会保留为一条独立学习路径。

::: generate explain-standard kind=explanation concepts=c-standard-version
面向刚开始学习 C 的读者，解释为什么“使用 GCC”不能替代“声明代码采用哪个 C 标准”，说明编译器版本、`-std=` 模式与 ISO C 标准版本的关系，并给出一条简短的编译命令。不得把编译器扩展描述为标准能力。
:::

## Mermaid 由 Gentorial 渲染

```mermaid
flowchart LR
    A[作者知识边界] --> B[概念锚点]
    B --> C[个性化主讲内容]
    A --> D[无 AI 时仍可阅读]
```

## 原有 Markdown 插件继续工作

脚注仍由项目原有的 `markdown-it-footnote` 提供。[^compat]

::: terminal title="zsh - C11"
<span class="prompt">$</span> cc -std=c11 hello.c -o hello

<span class="prompt">$</span> ./hello

hello, Gentorial
:::

[^compat]: Gentorial 通过原生 `markdown.config` 接入，没有替换脚注和作者自定义容器插件。
