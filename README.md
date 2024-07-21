# C

## Mdr's c tutorial

```c
#define INTRODUCTION 52
// 向着星辰与深渊，欢迎来到mdr的C语言教程！

#define INFORMATION 'C'
// B站账号 Minsecrus
// GitHub账号 Minsecrus
// QQ交流群 885719573

#include <stdio.h>
int main(int argc, char* argv[]){
    printf("%d %c", INTRODUCTION, INFORMATION);
    return 0;
}


```

此教程并非一时兴起而作，实则怀“为往圣继绝学，为万世开太平”之愿，深思熟虑打造而成  
得天独厚者，替天行善

[**总纲**](/总纲.md)

[**关于本教程**](/About.md)

## 本仓库文档规范

1. 标题由 **一级标题** 开始，依次递减
2. **强调** 用 `** **` 标识，*次强调* 用 `* *` 标识，`代码块` 用 `` ` ` `` 标识
3. `*** ***` `** **` `* *` `` ` ` `` 前后 **各空一个空格**；在中文标点前后的空格可省略
4. 引文和注释用 `>` 标识
5. 在一个 **术语(terminology)** 首次出现的时候在 **小括号** 内给出 **对应的外来语**，同时给出 **首字母缩略语(acronym)**。这些外来语通常对应 **概念(concept)** 的原文 ，希望有助于读者明确分辨词语的含义
   1. 为减少读者的记忆负担，下文分情况使用 **术语的汉语表述** 和 **首字母缩略语**：初始章节以前者为主，进阶章节以后者为主
   2. 为强调外来语对应左侧概念，括号左侧 **无空格**，右侧 *至少有一个空格*
   3. 大部分外来语单词摘自 [**维基百科**](https://zh.wikipedia.org) 和 [**C++ 参考手册**](https://en.cppreference.com/)

## 本仓库代码规范

1. 源文件后缀名为 `.c`，头文件后缀名为 `.h`
2. 标识符采用惯例命名，严禁用拼音
3. `if` `else` `for` `while` 必须跟随一条 **复合语句**，即用 **大括号** 包裹

## 鸣谢名单 （按时间顺序）

### 个人

+ **mdr** ([GitHub - Minsecrus](https://github.com/Minsecrus))
+ **@幻** ([GitHub - huan201](https://github.com/huan201))
+ **JuaJuanchi** ([GitHub - JuaJuanchi](https://github.com/Jua-Juanchi))
+ **Enoch.驊** ([GitHub - gngtwhh](https://github.com/gngtwhh))
+ **No** ([CNBlogs - Kroner](https://www.cnblogs.com/Kroner)) ([GitHub - Kroner](https://github.com/Kroner))
+ **xly** ([GitHub - LY-Xiang](https://github.com/LY-Xiang)) ([GitHub - hfsz2313](https://github.com/hfsz2313))
+ **革匊习习中** ([GitHub - Juvwxyz](https://github.com/Juvwxyz))
+ **FrankHB** ([GitHub - FrankHB](https://github.com/FrankHB))
+ **0x7E7**

### 组织

+ **艾猫工作室** （[aymao.com](https://www.aymao.com/)）

## 许可证

[**CC-BY-SA 4.0**](/LICENSE)

## 参考资料

+ [***ISO C standard***](https://open-std.org/JTC1/SC22/WG14/)
+ [***C++ 参考手册***](https://zh.cppreference.com/w/c/language)
+ ***C Primer Plus*** (ISBN 978-0-321-92842-9)
+ ***算法导论*** (ISBN 978-7-111-40701-0)
