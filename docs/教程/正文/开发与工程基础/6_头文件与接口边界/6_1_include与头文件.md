# 6.1 `#include` 与头文件

## `#include` 预处理指令

1. `#include <字符序列>`：按实现定义的方式搜索头或源文件。
2. `#include "字符序列"`：先按实现定义的方式搜索与当前源文件相关的位置；若没有找到，再按尖括号形式处理。

工程中通常用引号形式包含本项目头文件，用尖括号形式包含标准头和由构建系统配置搜索路径的外部头文件。具体搜索目录及先后顺序必须以目标实现和构建配置为准。

可以先把这个指令理解为：指定文件经过相应预处理后，其预处理记号出现在当前文件的包含点。下面的反例只用于展示效果；工程中不要包含 `.c` 文件。

::: code-group

```c [main.c]
#include <stdio.h>

int main(void) {
#include "arr.c"
    for(int i = 0; i < 10; i++){
        printf("%d ",arr[i]);
    }
    return 0;
}
```
```c [arr.c]
int arr[10] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10
};
```
:::

::: error 永远不要 `#include` 任何 `.c` 文件

每个 `.c` 文件都会变成一个翻译单元，被编译成目标文件。`#include` 一个 `.c` 文件，相当于把它的所有代码（包括函数定义等）复制到了另一个 `.c` 文件中。如果你在项目中还有其他地方用到了被包含的那个 `.c` 文件，同一个函数或对象就会被定义两次，链接器在合并所有目标文件时就会发现冲突并报错。

所以，应包含用于公开接口的头文件，而不是 `.c` 文件。头文件可以包含类型定义、宏定义和满足链接规则的其他定义，但通常不应放置会让多个翻译单元重复产生外部定义的函数或对象。

:::

经过翻译阶段 4 的处理（代码中没有表现出翻译阶段 1~3 的处理）之后，`main.c` 文件会变成：

```c
// stdio.h 内容

int main(void) {
    int arr[10] = {
1, 2, 3, 4, 5, 6, 7, 8, 9, 10
    };
    for(int i = 0; i < 10; i++){
        printf("%d ",arr[i]);
    }
    return 0;
}
```

输出：

::: terminal

1 2 3 4 5 6 7 8 9 10

:::
::: info 原封不动

预处理器不会按 C 语法检查被包含文件是否构成独立程序片段；语法分析看到的是包含完成后的记号序列。

也就是说，下面的代码可以通过编译：

::: code-group

```c [main.c]
#include <stdio.h>

int main() {
    int arr[10] = {
#include "arr.txt"
    }
    for(int i = 0; i < 10; i++){
        printf("%d ",arr[i]);
    }
    return 0;
}
```
```txt [arr.txt]
1, 2, 3, 4, 5, 6, 7, 8, 9, 10
```
但不要这样做。阅读代码的人可能弄不明白 `arr.txt` 文件的内容；如果其中内容出现错误，编译诊断可能跨越包含点和 `arr.txt`，加大调试难度；如果 `arr.txt` 增加一个数，`main.c` 中的数组声明也要相应修改，增大了耦合。

如果数据确实保存在 `_.txt` 当中，可以编写脚本程序，在编译时读取 `txt`文件，生成含有 `arr` 数组定义的代码，再将该代码包含到 `main.c` 文件中。

:::

:::

根据翻译阶段的规则，被包含的文件会被重新执行翻译阶段 1~4。当被包含的文件里面仍然有 `#include` 指令时，会嵌套包含。我们有时会遇到这种情况：A 文件包含 B 文件，B 文件包含 A 文件。这时就会出现循环包含的问题，如下：

::: code-group

```c [a.h]
#include "b.h"

void a(void);
```
```c [b.h]
#include "a.h"

void b(void);
```
```c [main.c]
#include "a.h"

int main(void) { /* ... */ }
```
:::

::: details 为什么会循环包含

从 `main.c` 开始看，它包含了 `a.h`，处理之后本该变成这样：

```c
// a.h 内容（记作代码①）

int main(void) { /* ... */ }
```
但是，包含的 `a.h` 文件要经过预处理，才能将其内容插入上述代码块中 `// a.h 内容` 所在之处。它又包含了 `b.h`，处理之后的 `a.h`（代码①）变成了这样：

```c
// b.h 内容（记作代码②）

void a(void);
```
包含的 `b.h` 文件也要经过预处理，才能将其内容插入上述代码块中 `// b.h 内容` 所在之处：

```c
// a.h 内容

void b(void);
```
这样，预处理的过程就会无限递归下去，如下面的流程图：

```mermaid
flowchart TB
    subgraph F1[main.c]
        direction TB
        A[a.h 预处理后内容]
        B[main 函数]
    end
    subgraph F2[a.h 第一次包含]
        direction TB
        C[b.h 预处理后内容]
        D[a.h 中内容]
    end
    subgraph F3[b.h 第一次包含]
        direction TB
        E[a.h 预处理后内容]
        F[b.h 中内容]
    end
    subgraph F4[a.h 第二次包含]
        direction TB
        G[b.h 预处理后内容]
        H[a.h 中内容]
    end
    A-->F2
    C-->F3
    E-->F4
    G-->F5[......]
```
:::

为避免这个问题，我们通常采取**头文件防护**的措施。典型的做法是在每个头文件的开头加上：

```c
#ifndef TOKEN
#define TOKEN
// some code
#endif
```
其中，`TOKEN` 是用来这个文件的独一无二的名字。

这样，第一次包含该头文件时，`TOKEN` 没有被定义，`#ifndef` 后面的代码会被执行。当第二次包含该头文件时，`TOKEN` 已经被定义，`#ifndef` 后面的代码不会被执行。流程图如下：

```mermaid
flowchart LR
    A{已经被包含？}
    B[true]
    C[false]
    D[返回内容]
    E[返回空白]
    A-->B
    A-->C
    C-->D
    B-->E
```
用上面的代码做例子：

::: code-group

```c [a.h]
#ifndef A_H_INCLUDED
#define A_H_INCLUDED
#include "b.h"

void a(void) { /* ... */ }
#endif // A_H_INCLUDED
```
```c [b.h]
#ifndef B_H_INCLUDED
#define B_H_INCLUDED
#include "a.h"

void b(void) { /* ... */ }
#endif // B_H_INCLUDED
```
```c [main.c]
#include "a.h"

int main(void) { /* ... */ }
```
:::

这样，`main.c` 经过预处理的步骤如下：

```c
// a.h 内容

int main(void) { /* ... */ }
```
上述代码中的 `a.h` 经过预处理后如下：

```c
// b.h 内容

void a(void) { /* ... */ }
```
此时定义了 `A_H_INCLUDED`。

上述代码中的 `b.h` 经过预处理后如下：

```c
// a.h 内容

void b(void) { /* ... */ }
```
此时定义了 `B_H_INCLUDED`。

上述代码中的 `a.h` 再次经过预处理。因为已经定义了 `A_H_INCLUDED`，`a.h` 内容为空，`b.h` 只剩以下内容：

```c
void b(void) { /* ... */ }
```
`a.h` 经过预处理之后的内容：

```c
void b(void) { /* ... */ }

void a(void) { /* ... */ }
```
`main.c` 经过预处理之后的内容：

```c
void b(void) { /* ... */ }

void a(void) { /* ... */ }

int main(void) { /*... */ }
```
也可以将上面的过程画成流程图：

```mermaid
flowchart TB
    subgraph F1[main.c]
        direction TB
        A[a.h 预处理后内容]
        B[main 函数]
    end
    subgraph F2[a.h 第一次包含]
        direction TB
        P11[/A_H_INCLUDED ?/]
        P12[false]
        C[b.h 预处理后内容]
        D[a.h 中内容]
        P13(设置 A_H_INCLUDED 为 true)
        P11-->P12
        P12-->C
        P12-->D
        P12-->P13
    end
    subgraph F3[b.h 第一次包含]
        direction TB
        P21[/B_H_INCLUDED ?/]
        P22[false]
        E[a.h 预处理后内容]
        F[b.h 中内容]
        P23(设置 B_H_INCLUDED 为 true)
        P21-->P22
        P22-->E
        P22-->F
        P22-->P23
    end
    subgraph F4[a.h 第二次包含]
        direction LR
        P31[/A_H_INCLUDED ?/]
        P32[true]
        P33[空白]
        P31-->P32
        P32-->P33
    end
    A-->F2
    C-->F3
    E-->F4
```
这样就防止了头文件的循环包含。

很多编译器实现了 `#pragma once` 指令，可以与上面的方法实现相同效果。可以同时采用这两种方式。
