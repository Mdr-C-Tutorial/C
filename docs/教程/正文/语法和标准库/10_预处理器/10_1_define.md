# 替换文本宏

## 1. 仿对象宏：`#define 标识符 替换列表`

将之后每一个出现的标识符替换成 `替换列表`（`替换列表` 可以为空），示例：

```c
#include <stdio.h>

#define NUM 100

int main(void){
    printf("%d", NUM);
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


程序将会输出 100

## 2. 仿函数宏

1. `#define 标识符（形参列表） 替换列表`

   当下文出现标识符紧跟一个“实参列表”的文本，将会被替换成 `替换列表`，同时 `替换列表` 中与形参列表中的形参相同的文本将会被替换成“实参列表”中对应的实参。

   示例：

   ```c
   #define max(a,b) (((a)>(b))?(a):(b))

   int main(void){
       printf("%d",max(1,2));
   }
   ```

   `max(1,2)` 会被替换成 `(((1)>(2))?(1):(2))`。

   因为宏是文本的替换，因此在替换过程中可能会导致一些运算符优先级问题，如下：

   ```c
   #define F(x) x + 2
   #define G(x) 3 * x

   int main(void){
       printf("%d", G(F(2)));
   }
   ```

   此时 `G(F(2))` 被替换成 `3 * F(2)`，`F(2)` 被替换成 `2 + 2`，因此 `G(F(2))` 会被替换成 `3 * 2 + 2`！可能会造成与预期不同的结果，造成程序的错误。

   解决方案是把宏以及宏里面所有形参加括号：

   ```c
   #define F(x) ((x) + 2)
   #define G(x) (3 * (x))
   ```

2. `#define 标识符（形参列表, ...） 替换列表`

   在替换列表中，额外实参会替换 `__VA_ARGS__`标识符。

   替换列表中可以包含标记序列 `__VA_OPT__(内容)`：当没有额外实参的时候，标记序列被替换为空，否则被替换为 `内容`。

   示例：

   ```c
   #define DEFINE_ARRAY(type, name, size, ...) type name[size] __VA_OPT__(= { __VA_ARGS__ })

   int main(void){
       DEFINE_ARRAY(int, arr, 4, 1, 2, 3, 4);
   }
   ```

   `DEFINE_ARRAY(int, arr, 4, 1, 2, 3, 4)` 会被替换成 `int arr[4] = {1, 2, 3, 4}`

3. `#` 运算符：把实参包含在引号中，变成一个字符串字面量。

   示例：

   ```c
   #define STR(a) #a

   int main(void){
       printf("%s is %s", STR(mdr), STR("cute"));
   }
   ```

   `STR(mdr)` 会被替换成 `"mdr"`

   `STR("cute")` 会被替换成 `"\"cute\""`

4. `##`` 运算符：连接两个记号。

   示例：

   ```c
   #define FUN(x) int fun_##x(){return x;}

   FUN(2)

   int main(void){
       printf("%d", fun_2());
   }
   ```

   `FUN(2)` 会被展开成 `int fun_2(){return 2;}`。

示例：

```c
#define FUN(x) int fun_##x(){ return x; }
#define OUTPUT(str, ...) printf(#str __VA_OPT__(,) __VA_ARGS__)
#include <stdio.h>
FUN(2)
int main(void){
    OUTPUT(%d, fun_2());
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


展开后的结果：

```c
#include <stdio.h>
int fun_2(){ return 2; }
int main(void){
    printf("%d", fun_2());
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


程序会输出`2`。

## 3. `#undef 标识符`

解除之前对 `标识符` 的定义

::: info

宏不能实现真正的递归和重载。

:::

::: tip

由于预处理指令**占据一行**，所以当宏的内容超过一行时，需要在行尾加上 `\` 来实现宏的跨行。

:::
