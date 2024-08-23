# C

## Mdr's C tutorial

```c
#define INTRODUCTION 52
// 向着星辰与深渊，欢迎来到 mdr 的 C 语言教程！

#define INFORMATION 'C'
// B 站账号 Minsecrus
// GitHub 账号 Minsecrus
// QQ 交流群 885719573

#include <stdio.h>
int main(int argc, char* argv[]){
    printf("%d %c", INTRODUCTION, INFORMATION);
    return 0;
}
```

1. 这里是 **Mdr's C Tutorial**，欢迎来这里共同学习
2. 此仓库中文档的定位是 **教程讲义**：
   对于贡献者，意味着无需引导和废话，只需讲干货；
   对于读者，意味着阅读这些文档可以快速获得与 **C 语言** 以及 **计算机科学** 相关知识
3. 本教程以及本文档的目标：在 **准确** 的基础上力争 **简洁** 和 **生动**
4. 此教程并非一时兴起而作，实则怀“为往圣继绝学，为万世开太平”之愿，深思熟虑打造而成
5. 得天独厚者，替天行善

[**总纲**](/教程/总纲.md)

[**关于本教程**](/About.md)

## 文档规范

1. 标题由 **一级标题** 开始，依次递减；
2. 标题层级不应过多，如果出现该情况，可以拆分成多篇文章；  
   一般地，标题层级不应超过 **四** 级；
3. `/教程` 和 `/杂项/工具和环境` 文件夹里的文档，除一级标题之外的标题应有 **编号**：
   1. 二级标题前应有 `1.` `2.` 等编号，数字后加小数点；
   2. 三级标题前应有 `1.1` `1.2` 等编号，末尾不要加小数点；
   3. 四级标题前应有 `1.1.1` `1.1.2` 等编号，末尾不要加小数点；
4. **代码块** 和 **列表** 前后各有一个空行；文件结尾要有一个空行；文件任何位置不得存在连续两个空行；
5. 段落和段落之间有一个空行；只换行不换段落，无空行，在前一行结尾加两个空格；
6. **强调** 用 `** **` 标识，*次强调* 用 `* *` 标识，`代码块` 用 `` ` ` `` 标识；
7. 引文和注释用 `>` 标识；书名和文章名用 `*** ***` 标识；
8. `*** ***` `** **` `* *` `` ` ` `` 前后 **各空一个空格**，但中文标点前后无需加空格；
9. 中西文字符，或者中文和数字之间加一个空格；
10. 在中文上下文中用全角标点，比如全角小括号，全角冒号等；
11. 要正确使用标点符号，分清 **顿号**、**逗号**、**分号** 和 **句号**，尤其避免一逗到底；
12. 分清 **的地得** 和 **他她它祂怹**
13. 仅在必要时使用 **数量词**（尤其是 **一个**）~~你是一个一个一个~~
14. 在 **术语** (Terminology) 首次出现的时候在 **半角小括号** 内给出 **对应的外来语**，同时给出 **首字母缩略语** (Acronym)。这些外来语通常对应 **概念** (Concept) 的原文 ，希望有助于读者明确分辨词语的含义：
    1. 为减少读者的记忆负担，下文分情况使用 **术语的汉语表述** 和 **首字母缩略语**：初始章节以前者为主，进阶章节以后者为主
    2. 外来语和缩略语可能视情况在不同章节多次出现
    3. 括号外左侧右侧各空一个空格，括号内无空格
    4. 外来语 **不能** 带斜体、加粗、下划线、删除线和代码块——用默认的字体；
    5. 大部分外来语单词摘自 [维基百科](https://zh.wikipedia.org) 和 [C++ 参考手册](https://en.cppreference.com/)；
15. **专有名词** 需 **大写**；
16. 图片统一放在 `/images/` 目录下，并采用 **绝对路径** 引入：
    1. 截图统一采用 **PNG** 格式，命名为三位数字，如：`001.png`；  
    2. 拍照统一采用 **JPEG** 格式，命名为不带前导零的数字，如：`1.jpg`；
17. 在每篇文档结尾写出引用内容的出处；
18. 语言风格：
    1. 使用 **正式** 的中文白话，尽量 **不用** 口头语；
    2. 尽量 **不用** 反问，设问和借喻；
    3. 用清晰的修饰限定成分，避免含混不清；不用过于夸张的修饰限定成分；有数据支持的时候要列举数据；
    4. 避免带有主观性的措辞；避免连续的词表示相同意思；
    5. 保证整个仓库中所有文档 **语言风格的一致性**。

## 代码规范

1. 如果是项目代码，需要构建系统完成编译运行，需将必要的相关文件放入对应目录，例如 `CMakeLists.txt` 和 `xmake.lua` 等；
2. 源文件后缀名为 `.c`，头文件后缀名为 `.h`；
3. 标识符采用惯例命名，不允许使用拼音；
4. `if` `else` `for` `while` 必须跟随一条 **复合语句**。

## 对预期读者的要求

1. 独立思考；
2. 勤于搜索（尤其是出现较多关联概念时）；
3. 理解中文白话，最好有一定的英语水平（建议达到 **CEFR B2**）但不强制，最好有一定的数学水平（高中数学 + 数理逻辑基础 + 微积分 + 基础的概率论 + 基础的数论 + 基础的图论）但不强制；

## 鸣谢名单（按时间顺序）

### 个人（贡献者）

+ **mdr** （[GitHub - Minsecrus](https://github.com/Minsecrus)）
+ **@幻** （[GitHub - huan201](https://github.com/huan201)）
+ **JuaJuanchi** （[GitHub - JuaJuanchi](https://github.com/Jua-Juanchi)）
+ **Enoch.驊** （[GitHub - gngtwhh](https://github.com/gngtwhh)）
+ **No** （[CNBlogs - Kroner](https://www.cnblogs.com/Kroner)）（[GitHub - Kroner](https://github.com/Kroner)）
+ **xly** （[GitHub - LY-Xiang](https://github.com/LY-Xiang)）（[GitHub - hfsz2313](https://github.com/hfsz2313)）
+ **革匊习习中** （[GitHub - Juvwxyz](https://github.com/Juvwxyz)）
+ **0x7E7**
+ **FHU-yezi** （[GitHub - FHU-yezi](https://github.com/FHU-yezi)）
+ **菲露露** （[GitHub - LovelyLavender4](https://github.com/lovelylavender4)）
+ **XIAOYI12** （[GitHub - xiaoyi1212](https://github.com/xiaoyi1212)）
+ **WHITE TREE**（[GitHub - VinbeWan](https://github.com/VinbeWan)）
+ **A4-Tacks**（[GitHub - A4-Tacks](https://github.com/A4-Tacks)）

### 组织

+ **艾猫工作室** （[aymao.com](https://www.aymao.com/)）

### 特别鸣谢

+ **FrankHB** （[GitHub - FrankHB](https://github.com/FrankHB)）
+ **jajuju**
+ **Fresky** （[GitHub - FreskyZ](https://github.com/FreskyZ)）
+ **兔兔**

## 许可证

[**CC-BY-SA 4.0**](/LICENSE)

## 参考资料

+ [***ISO C standard***](https://open-std.org/JTC1/SC22/WG14/)
+ [***C++ 参考手册***](https://zh.cppreference.com/w/c/language)
+ ***C Primer Plus*** （ISBN 978-0-321-92842-9）
+ ***算法导论*** （ISBN 978-7-111-40701-0）
+ [***Let's Build a Simple Database***](https://cstack.github.io/db_tutorial/)
+ [***pldocs***](https://github.com/FrankHB/pl-docs/blob/master/zh-CN)
