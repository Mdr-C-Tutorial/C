# 梦猫大典 - Mdr-C-Tutorial 教程讲义及示例代码开源仓库

## 仓库内容概述

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

1. 这里是 **Mdr's C Tutorial**，欢迎来这里共同学习！
2. 此仓库中文档的定位是**教程讲义**：  
   对于贡献者，意味着无需引导和废话，只需讲干货；  
   对于读者，意味着阅读这些文档可以快速获得与**C 语言**以及**计算机科学**相关知识；
3. 本教程以及本讲义的目标：在**准确**的基础上力争**简洁**和**生动**；
4. 此教程并非一时兴起而作，实则怀「为往圣继绝学，为万世开太平」之愿，深思熟虑打造而成。
5. 得天独厚者，替天行善！

## 构建与部署

1. 本文档使用 [VitePress](https://vitejs.cn/vitepress/) 构建，需最新版 yarn 以在本地 build 和 dev。
2. 本仓库采用 GitHub Actions 在每次提交时自动构建和部署。

## 文档规范

该栏为本仓库 markdown 文档的规范。

1. 标题由**一级标题**开始，依次递减；
2. 标题层级不应过多，如果出现该情况，可以拆分成多篇文章；  
   一般地，标题层级不应超过**四**级；
3. `/教程` 和 `/杂项/工具和环境` 文件夹里的文档，除一级标题之外的标题应有**编号**：
   1. 二级标题前应有 `1.` `2.` 等编号，数字后加小数点；
   2. 三级标题前应有 `1.1` `1.2` 等编号，末尾不要加小数点；
   3. 四级标题前应有 `1.1.1` `1.1.2` 等编号，末尾不要加小数点；
4. **代码块**和**列表**前后各有一个空行；文件结尾要有一个空行；文件任何位置不得存在连续两个空行；
5. 段落和段落之间有一个空行；只换行不换段落，无空行，在前一行结尾加两个空格；
6. **强调**用 `** **` 标识，_次强调_ 用 `_ _` 标识，`代码块` 用 `` ` ` `` 标识；
7. 引文和注释用 `>` 标识；书名和文章名用 `** **` 标识；
8. `` ` ` `` `$ $` 前后**各空一个空格**，但中文标点前后无需加空格；
9. 中西文字符，或者中文和数字之间加一个空格；
10. 在中文上下文中用全角标点，比如全角小括号，全角冒号等；不要省略段尾句号；
11. 要正确使用标点符号，分清**顿号**、**逗号**、**分号**和**句号**，尤其避免一逗到底；
12. 分清**的地得**和**他她它**
13. 仅在必要时使用**数量词**（尤其警惕**一个**的不恰当使用）
14. 在**术语** (Terminology) 首次出现的时候在**半角小括号**内给出**对应的外来语**，同时给出**首字母缩略语** (Acronym)。这些外来语通常对应**概念** (Concept) 的原文 ，希望有助于读者明确分辨词语的含义：
    1. 为减少读者的记忆负担，下文分情况使用**术语的汉语表述**和**首字母缩略语**：初始章节以前者为主，进阶章节以后者为主
    2. 外来语和缩略语可能视情况在不同章节多次出现
    3. 括号外左侧右侧各空一个空格，括号内无空格
    4. 外来语**不能**带斜体、加粗、下划线、删除线和代码块——用默认的字体；
    5. 大部分外来语单词摘自 [维基百科](https://zh.wikipedia.org) 和 [C++ 参考手册](https://en.cppreference.com/)；
15. 专有名词需大写；
16. 图片统一放在 `/images/` 目录下，并采用**绝对路径**引入：
    1. 截图统一采用 **PNG** 格式，命名为三位数字，如：`001.png`；
    2. 拍照统一采用 **JPEG** 格式，命名为不带前导零的数字，如：`1.jpg`；
17. 在每篇文档结尾写出引用内容的出处；
18. 语言风格：
    1. 使用正式的中文白话，不用口头语；
    2. 杜绝语病；
    3. 尽量不用反问，设问和借喻；
    4. 用清晰的修饰限定成分，避免含混不清；不用过于夸张的修饰限定成分；有数据支持时要列举数据；
    5. 避免带有主观性的措辞；避免连续的词表示相同意思；
    6. 保证仓库中所有文档语言风格一致。

## 代码规范

该栏为本仓库 C 语言代码的规范。

1. 如果是项目代码，需要构建系统完成编译运行，须将必要的相关文件放入对应目录，如 `CMakeLists.txt` 和 `xmake.lua` 等；
2. 源文件后缀名为 `.c`，头文件后缀名为 `.h`；
3. 代码需保证在新版 GCC 和 Clang 上以新版 C 语言标准编译通过，“语法和标准库”部分的代码还需做到开启 `-Wall` 和 `-Wextra` 无警告；
4. 代码格式化样式：基于 **LLVM** 样式，以 **4 空格**缩进；

   ```json
   {
     "BasedOnStyle": "LLVM",
     "IndentWidth": 4
   }
   ```

5. 标识符的命名：
   1. 使用惯例，尤其禁止使用拼音；
   2. 宏名所有字母均大写，单词之间以下划线分割；
   3. 其余标识符所有字母均小写，单词之间以下划线分割；
6. `if` `else` `for` `while` 必须跟随**复合语句**。

## 对预期读者的要求

1. 独立思考；
2. 勤于搜索（尤其是出现较多关联概念时）；
3. 理解中文白话，最好有一定的英语水平（建议达到 CEFR B2），最好有一定的数学水平（高中数学 + 数理逻辑基础 + 微积分 + 基础的概率论 + 基础的数论 + 基础的图论）。

## 许可证

[**CC-BY-NC-SA 4.0**](/LICENSE)

## 参考资料

- [**_ISO C standard_**](https://open-std.org/JTC1/SC22/WG14/)
- [**_C++ 参考手册_**](https://zh.cppreference.com/w/c/language)
- **_C Primer Plus_** （ISBN 978-0-321-92842-9）
- **_算法导论_** （ISBN 978-7-111-40701-0）
- [**_Let's Build a Simple Database_**](https://cstack.github.io/db_tutorial/)
- [**_pldocs_**](https://github.com/FrankHB/pl-docs/blob/master/zh-CN)
- **_陶哲轩实分析_** （ISBN 978-7-115-48025-5）
- [**OSDEV.org**](https://wiki.osdev.org/)
- [**Hello 算法**](https://hello-algo.com/)
