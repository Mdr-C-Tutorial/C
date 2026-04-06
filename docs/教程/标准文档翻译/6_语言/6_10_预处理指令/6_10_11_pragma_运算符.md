# 6.10.11 pragma 运算符

## 语义

1. 形如

::: details 形式化语法

```bnf
_Pragma ( string-literal )
```

:::

的一元运算符表达式按如下方式处理：

2. 先对该字符串字面量执行“反字符串化”：

- 删除任何编码前缀；
- 删除首尾双引号；
- 把每个转义序列 `\"` 替换成一个双引号；
- 把每个转义序列 `\\` 替换成一个反斜杠。

3. 得到的字符序列再经过翻译阶段 `3`，生成预处理记号。
4. 这些预处理记号会被当作 pragma 指令中的 `pp-tokens` 来执行。
5. 原来这个一元运算符表达式里的四个预处理记号会被移除。

## 示例

下面的指令：

```c
#pragma listing on "..\listing.dir"
```

也可以写成：

```c
_Pragma ("listing on \"..\\listing.dir\"")
```

后一种写法，无论是像上面这样直接写出，还是通过宏替换得到，处理方式都相同。例如：

```c
#define LISTING(x) PRAGMA(listing on #x)
#define PRAGMA(x) _Pragma(#x)

LISTING (..\listing.dir)
```
