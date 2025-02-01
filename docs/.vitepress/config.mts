import { defineConfig } from "vitepress";
import { withMermaid } from "vitepress-plugin-mermaid";
import mathjax3 from "markdown-it-mathjax3";

const customElements = [
  "math",
  "maction",
  "maligngroup",
  "malignmark",
  "menclose",
  "merror",
  "mfenced",
  "mfrac",
  "mi",
  "mlongdiv",
  "mmultiscripts",
  "mn",
  "mo",
  "mover",
  "mpadded",
  "mphantom",
  "mroot",
  "mrow",
  "ms",
  "mscarries",
  "mscarry",
  "mscarries",
  "msgroup",
  "mstack",
  "mlongdiv",
  "msline",
  "mstack",
  "mspace",
  "msqrt",
  "msrow",
  "mstack",
  "mstack",
  "mstyle",
  "msub",
  "msup",
  "msubsup",
  "mtable",
  "mtd",
  "mtext",
  "mtr",
  "munder",
  "munderover",
  "semantics",
  "math",
  "mi",
  "mn",
  "mo",
  "ms",
  "mspace",
  "mtext",
  "menclose",
  "merror",
  "mfenced",
  "mfrac",
  "mpadded",
  "mphantom",
  "mroot",
  "mrow",
  "msqrt",
  "mstyle",
  "mmultiscripts",
  "mover",
  "mprescripts",
  "msub",
  "msubsup",
  "msup",
  "munder",
  "munderover",
  "none",
  "maligngroup",
  "malignmark",
  "mtable",
  "mtd",
  "mtr",
  "mlongdiv",
  "mscarries",
  "mscarry",
  "msgroup",
  "msline",
  "msrow",
  "mstack",
  "maction",
  "semantics",
  "annotation",
  "annotation-xml",
  "mjx-container",
  "mjx-assistive-mml",
];
// https://vitepress.dev/reference/site-config
export default withMermaid(
  defineConfig({
    title: "Mdr-C-Tutorial",
    titleTemplate: "Mdr-C-Tutorial",
    description: "Mdr C Tutorial Repository",
    lang: "zh-CN",
    base: "/C/",
    cleanUrls: true,
    cacheDir: ".vitepress/cache",
    metaChunk: true,
    lastUpdated: true,
    themeConfig: {
      nav: [
        {
          text: "Home",
          link: "/",
        },
        {
          text: "总纲",
          link: "/教程/正文/总纲",
        },
        {
          text: "关于",
          link: "/About",
        },
      ],

      sidebar: {
        "/教程/正文/": [
          {
            text: "教程目录",
            items: [
              {
                text: "总纲",
                link: "/教程/正文/总纲",
              },
              {
                text: "语法和标准库",
                collapsed: false,
                items: [
                  {
                    text: "1. 在开始编程之前",
                    link: "/教程/正文/语法和标准库/1_在开始编程之前",
                  },
                  {
                    text: "2. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序"
                  },
                  {
                    text: "3. 基本概念",
                    link: "/教程/正文/语法和标准库/3_基本概念",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.2 对象",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_2_对象",
                        collapsed: false,
                        items: [
                          {
                            text: "3.2.1 类型",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_2_对象/3_2_1_类型",
                          },
                          {
                            text: "3.2.2 声明和初始化",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_2_对象/3_2_2_声明和初始化",
                          },
                        ],
                      },
                      {
                        text: "3.3 遵从性和行为",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_3_遵从性和行为",
                      },
                      {
                        text: "3.4 翻译单元",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_4_翻译单元",
                      },
                      {
                        text: "3.5 翻译阶段",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_5_翻译阶段",
                      },
                      {
                        text: "3.6 标识符",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_6_标识符",
                        collapsed: false,
                        items: [
                          {
                            text: "3.6.1 保留标识符",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_6_标识符/3_6_1_保留标识符",
                          },
                          {
                            text: "3.6.2 作用域",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_6_标识符/3_6_2_作用域",
                          },
                        ],
                      },
                      {
                        text: "3.7 ASCII 码",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_7_ASCII码",
                      },
                    ]
                  },
                  {
                    text: "4. 表达式",
                    link: "/教程/正文/语法和标准库/4_表达式",
                    collapsed: true,
                    items: [
                      {
                        text: "4.1 运算符",
                        link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符",
                        collapsed: false,
                        items: [
                          {
                            text: "4.1.1 算术运算符",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_1_算术运算符",
                          },
                          {
                            text: "4.1.2 逻辑运算符",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_2_逻辑运算符",
                          },
                          {
                            text: "4.1.3 赋值运算符",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_3_赋值运算符",
                          },
                          {
                            text: "4.1.4 关系运算符",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_4_关系运算符",
                          },
                          {
                            text: "4.1.5 三元运算符",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_5_三元运算符",
                          },
                          {
                            text: "4.1.6 类型转换运算符",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_6_类型转换运算符",
                          },
                          {
                            text: "4.1.7 逗号运算符",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_7_逗号运算符",
                          },
                          {
                            text: "4.1.8 sizeof",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_8_sizeof",
                          },
                          {
                            text: "4.1.9 其他运算符",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_9_其他运算符",
                          },
                          {
                            text: "4.1.10 优先级 结合性",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_10_优先级_结合性",
                          },
                          {
                            text: "4.1.11 运算符宏",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_11_运算符宏",
                          },
                          {
                            text: "4.1.12 代用记号",
                            link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符/4_1_12_代用记号",
                          },
                        ],
                      },
                      {
                        text: "4.2 常量和字面量",
                        link: "/教程/正文/语法和标准库/4_表达式/4_2_常量和字面量",
                        collapsed: false,
                        items: [
                          {
                            text: "4.2.1 true false",
                            link: "/教程/正文/语法和标准库/4_表达式/4_2_常量和字面量/4_2_1_true_false",
                          },
                          {
                            text: "4.2.2 整数常量 浮点常量 字符常量",
                            link: "/教程/正文/语法和标准库/4_表达式/4_2_常量和字面量/4_2_2_整数常量_浮点常量_字符常量",
                          },
                          {
                            text: "4.2.3 字符串字面量",
                            link: "/教程/正文/语法和标准库/4_表达式/4_2_常量和字面量/4_2_3_字符串字面量",
                          },
                        ],
                      },
                      {
                        text: "4.3 求值",
                        link: "/教程/正文/语法和标准库/4_表达式/4_3_求值",
                        collapsed: false,
                        items: [
                          {
                            text: "4.3.1 求值和副作用",
                            link: "/教程/正文/语法和标准库/4_表达式/4_3_求值/4_3_1_求值和副作用",
                          },
                          {
                            text: "4.3.2 求值顺序",
                            link: "/教程/正文/语法和标准库/4_表达式/4_3_求值/4_3_2_求值顺序",
                          },
                        ],
                      },
                      {
                        text: "4.4 泛型选择",
                        link: "/教程/正文/语法和标准库/4_表达式/4_4_泛型选择",
                      },
                    ],
                  },
                  {
                    text: "5. 语句",
                    link: "/教程/正文/语法和标准库/5_语句",
                    collapsed: true,
                    items: [
                      {
                        text: "5.1 表达式语句",
                        link: "/教程/正文/语法和标准库/5_语句/5_1_表达式语句",
                      },
                      {
                        text: "5.2 选择语句",
                        link: "/教程/正文/语法和标准库/5_语句/5_2_选择语句",
                        collapsed: false,
                        items: [
                          {
                            text: "5.2.1 if else",
                            link: "/教程/正文/语法和标准库/5_语句/5_2_选择语句/5_2_1_if_else",
                          },
                          {
                            text: "5.2.2 switch",
                            link: "/教程/正文/语法和标准库/5_语句/5_2_选择语句/5_2_2_switch",
                          },
                        ],
                      },
                      {
                        text: "5.3 循环语句",
                        link: "/教程/正文/语法和标准库/5_语句/5_3_循环语句",
                        collapsed: false,
                        items: [
                          {
                            text: "5.3.1 while",
                            link: "/教程/正文/语法和标准库/5_语句/5_3_循环语句/5_3_1_while",
                          },
                          {
                            text: "5.3.2 do while",
                            link: "/教程/正文/语法和标准库/5_语句/5_3_循环语句/5_3_2_do_while",
                          },
                          {
                            text: "5.3.3 for",
                            link: "/教程/正文/语法和标准库/5_语句/5_3_循环语句/5_3_3_for",
                          },
                        ],
                      },
                      {
                        text: "5.4 跳转语句",
                        link: "/教程/正文/语法和标准库/5_语句/5_4_跳转语句",
                        collapsed: false,
                        items: [
                          {
                            text: "5.4.1 break continue",
                            link: "/教程/正文/语法和标准库/5_语句/5_4_跳转语句/5_4_1_break_continue",
                          },
                          {
                            text: "5.4.2 return",
                            link: "/教程/正文/语法和标准库/5_语句/5_4_跳转语句/5_4_2_return",
                          },
                          {
                            text: "5.4.3 goto",
                            link: "/教程/正文/语法和标准库/5_语句/5_4_跳转语句/5_4_3_goto",
                          },
                        ],
                      },
                    ],
                  },
                  {
                    text: "6. 函数",
                    link: "/教程/正文/语法和标准库/6_函数",
                    collapsed: true,
                    items: [
                      {
                        text: "6.1 函数声明",
                        link: "/教程/正文/语法和标准库/6_函数/6_1_函数声明",
                      },
                      {
                        text: "6.2 可变参数",
                        link: "/教程/正文/语法和标准库/6_函数/6_2_可变参数",
                      },
                    ],
                  },
                  {
                    text: "7. 派生类型",
                    link: "/教程/正文/语法和标准库/7_派生类型",
                    collapsed: true,
                    items: [
                      {
                        text: "7.1 数组",
                        link: "/教程/正文/语法和标准库/7_派生类型/7_1_数组",
                      },
                      {
                        text: "7.2 指针",
                        link: "/教程/正文/语法和标准库/7_派生类型/7_2_指针",
                      },
                      {
                        text: "7.3 结构体",
                        link: "/教程/正文/语法和标准库/7_派生类型/7_3_结构体",
                      },
                      {
                        text: "7.4 联合体",
                        link: "/教程/正文/语法和标准库/7_派生类型/7_4_联合体",
                      },
                      {
                        text: "7.5 原子类型",
                        link: "/教程/正文/语法和标准库/7_派生类型/7_5_原子类型",
                      },
                    ],
                  },
                  {
                    text: "8. 枚举",
                    link: "/教程/正文/语法和标准库/8_枚举",
                  },
                  {
                    text: "9. 对象",
                    link: "/教程/正文/语法和标准库/9_对象",
                    collapsed: true,
                    items: [
                      {
                        text: "9.1 内存模型",
                        link: "/教程/正文/语法和标准库/9_对象/9_1_内存模型",
                      },
                      {
                        text: "9.2 对象表示",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                        collapsed: false,
                        items: [
                          {
                            text: "9.2.1 对齐",
                            link: "/教程/正文/语法和标准库/9_对象/9_2_对象表示/9_2_1_对齐",
                          },
                          {
                            text: "9.2.2 alignof",
                            link: "/教程/正文/语法和标准库/9_对象/9_2_对象表示/9_2_2_alignof",
                          },
                        ],
                      },
                      {
                        text: "9.3 生存期",
                        link: "/教程/正文/语法和标准库/9_对象/9_3_生存期",
                      },
                      {
                        text: "9.4 声明",
                        link: "/教程/正文/语法和标准库/9_对象/9_4_声明",
                        collapsed: false,
                        items: [
                          {
                            text: "9.4.1 typeof",
                            link: "/教程/正文/语法和标准库/9_对象/9_4_声明/9_4_1_typeof",
                          },
                          {
                            text: "9.4.2 constexpr",
                            link: "/教程/正文/语法和标准库/9_对象/9_4_声明/9_4_2_constexpr",
                          },
                          {
                            text: "9.4.3 const volatile",
                            link: "/教程/正文/语法和标准库/9_对象/9_4_声明/9_4_3_const_volatile",
                          },
                          {
                            text: "9.4.4 restrict",
                            link: "/教程/正文/语法和标准库/9_对象/9_4_声明/9_4_4_restrict",
                          },
                          {
                            text: "9.4.5 alignas",
                            link: "/教程/正文/语法和标准库/9_对象/9_4_声明/9_4_5_alignas",
                          },
                          {
                            text: "9.4.6 static_assert",
                            link: "/教程/正文/语法和标准库/9_对象/9_4_声明/9_4_6_static_assert",
                          },
                          {
                            text: "9.4.7 存储期 链接",
                            link: "/教程/正文/语法和标准库/9_对象/9_4_声明/9_4_7_存储期 链接",
                          },
                          {
                            text: "9.4.8 定义",
                            link: "/教程/正文/语法和标准库/9_对象/9_4_声明/9_4_8_定义",
                          },
                        ],
                      },
                      {
                        text: "9.5 typedef",
                        link: "/教程/正文/语法和标准库/9_对象/9_5_typedef",
                      },
                      {
                        text: "9.6 属性",
                        link: "/教程/正文/语法和标准库/9_对象/9_6_属性",
                      },
                    ],
                  },
                  {
                    text: "10. 预处理器",
                    link: "/教程/正文/语法和标准库/10_预处理器",
                    collapsed: true,
                    items: [
                      {
                        text: "10.1 define",
                        link: "/教程/正文/语法和标准库/10_预处理器/10_1_define",
                      },
                      {
                        text: "10.2 include",
                        link: "/教程/正文/语法和标准库/10_预处理器/10_2_include",
                      },
                      {
                        text: "10.3 if n def",
                        link: "/教程/正文/语法和标准库/10_预处理器/10_3_if_n_def",
                      },
                      {
                        text: "10.4 error warning",
                        link: "/教程/正文/语法和标准库/10_预处理器/10_4_error_warning",
                      },
                      {
                        text: "10.5 embed",
                        link: "/教程/正文/语法和标准库/10_预处理器/10_5_embed",
                      },
                      {
                        text: "10.6 pragma",
                        link: "/教程/正文/语法和标准库/10_预处理器/10_6_pragma",
                      },
                      {
                        text: "10.7 line",
                        link: "/教程/正文/语法和标准库/10_预处理器/10_7_line",
                      },
                    ],
                  },
                  {
                    text: "11. 标准库概述",
                    link: "/教程/正文/语法和标准库/11_标准库概述",
                  },
                  {
                    text: "12. 预定义宏",
                    link: "/教程/正文/语法和标准库/12_预定义宏",
                    collapsed: true,
                    items: [
                      {
                        text: "12.1 <stddef.h>",
                        link: "/教程/正文/语法和标准库/12_预定义宏/12_1_stddef",
                      },
                      {
                        text: "12.2 <stdint.h>",
                        link: "/教程/正文/语法和标准库/12_预定义宏/12_2_stdint",
                      },
                      {
                        text: "12.3 <inttypes.h>",
                        link: "/教程/正文/语法和标准库/12_预定义宏/12_3_inttypes",
                      },
                      {
                        text: "12.4 <limits.h>",
                        link: "/教程/正文/语法和标准库/12_预定义宏/12_4_limits",
                      },
                      {
                        text: "12.5 功能特性测试宏",
                        link: "/教程/正文/语法和标准库/12_预定义宏/12_5_功能特性测试宏",
                      },
                    ],
                  },
                  {
                    text: "13. 错误处理",
                    link: "/教程/正文/语法和标准库/13_错误处理",
                    collapsed: true,
                    items: [
                      {
                        text: "13.1 <assert.h>",
                        link: "/教程/正文/语法和标准库/13_错误处理/13_1_assert",
                      },
                      {
                        text: "13.2 <errno.h>",
                        link: "/教程/正文/语法和标准库/13_错误处理/13_2_errno",
                      },
                      {
                        text: "13.3 <signal.h>",
                        link: "/教程/正文/语法和标准库/13_错误处理/13_3_signal",
                      },
                    ],
                  },
                  {
                    text: "14. 输入输出",
                    link: "/教程/正文/语法和标准库/14_输入输出",
                    collapsed: true,
                    items: [
                      {
                        text: "14.1 IO 流",
                        link: "/教程/正文/语法和标准库/14_输入输出/14_1_IO流",
                      },
                      {
                        text: "14.2 文件操作",
                        link: "/教程/正文/语法和标准库/14_输入输出/14_2_文件操作",
                      },
                      {
                        text: "14.3 无格式/有格式输入输出",
                        link: "/教程/正文/语法和标准库/14_输入输出/14_3_无格式有格式输入输出",
                      },
                      {
                        text: "14.4 文件定位",
                        link: "/教程/正文/语法和标准库/14_输入输出/14_4_文件定位",
                      },
                      {
                        text: "14.5 文件操作",
                        link: "/教程/正文/语法和标准库/14_输入输出/14_5_文件操作",
                      },
                    ],
                  },
                  {
                    text: "15. 动态内存管理",
                    link: "/教程/正文/语法和标准库/15_动态内存管理",
                  },
                  {
                    text: "16. 字符/字符串库",
                    link: "/教程/正文/语法和标准库/16_字符字符串库",
                    collapsed: true,
                    items: [
                      {
                        text: "16.1 <ctype.h>",
                        link: "/教程/正文/语法和标准库/16_字符字符串库/16_1_ctype",

                      },
                      {
                        text: "16.2 <string.h>",
                        link: "/教程/正文/语法和标准库/16_字符字符串库/16_2_string",
                      },
                    ],
                  },
                  {
                    text: "17. 数学库",
                    link: "/教程/正文/语法和标准库/17_数学库",
                    collapsed: true,
                    items: [
                      {
                        text: "17.1 <math.h>",
                        link: "/教程/正文/语法和标准库/17_数学库/17_1_math",
                      },
                      {
                        text: "17.2 <complex.h>",
                        link: "/教程/正文/语法和标准库/17_数学库/17_2_complex",
                      },
                      {
                        text: "17.3 <tgmath.h>",
                        link: "/教程/正文/语法和标准库/17_数学库/17_3_tgmath",
                      },
                      {
                        text: "17.4 随机数生成",
                        link: "/教程/正文/语法和标准库/17_数学库/17_4_随机数生成",
                      },
                      {
                        text: "17.5 <stdbit.h>",
                        link: "/教程/正文/语法和标准库/17_数学库/17_5_stdbit",
                      },
                      {
                        text: "17.6 <stdckdint.h>",
                        link: "/教程/正文/语法和标准库/17_数学库/17_6_stdckdint",
                      },
                      {
                        text: "17.7 <fenv.h>",
                        link: "/教程/正文/语法和标准库/17_数学库/17_7_fenv",
                      },
                    ],
                  },
                  {
                    text: "18. 算法库",
                    link: "/教程/正文/语法和标准库/18_算法库",
                  },
                  {
                    text: "19. 时间日期库",
                    link: "/教程/正文/语法和标准库/19_时间日期库",
                  },
                  {
                    text: "20. 本地化库",
                    link: "/教程/正文/语法和标准库/20_本地化库"
                  },
                  {
                    text: "21. 程序支持",
                    link: "/教程/正文/语法和标准库/21_程序支持",
                    collapsed: true,
                    items: [
                      {
                        text: "21.1 终止程序",
                        link: "/教程/正文/语法和标准库/21_程序支持/21_1_终止程序",
                      },
                      {
                        text: "21.2 环境访问",
                        link: "/教程/正文/语法和标准库/21_程序支持/21_2_环境访问",
                      },
                      {
                        text: "21.3 跳转",
                        link: "/教程/正文/语法和标准库/21_程序支持/21_3_跳转",
                      },
                    ],
                  },
                  {
                    text: "22. 并发支持",
                    link: "/教程/正文/语法和标准库/22_并发支持",
                    collapsed: true,
                    items: [
                      {
                        text: "22.1 线程",
                        link: "/教程/正文/语法和标准库/22_并发支持/22_1_线程",
                      },
                      {
                        text: "22.2 原子操作",
                        link: "/教程/正文/语法和标准库/22_并发支持/22_2_原子操作",
                      },
                      {
                        text: "22.3 互斥",
                        link: "/教程/正文/语法和标准库/22_并发支持/22_3_互斥",
                      },
                      {
                        text: "22.4 条件变量",
                        link: "/教程/正文/语法和标准库/22_并发支持/22_4_条件变量",
                      },
                      {
                        text: "22.5 线程局部存储",
                        link: "/教程/正文/语法和标准库/22_并发支持/22_5_线程局部存储",
                      },
                    ],
                  },
                  {
                    text: "23. 标准库扩展",
                    link: "/教程/正文/语法和标准库/23_标准库扩展"
                  },
                ],
              },
              {
                text: "数据结构与算法",
                collapsed: false,
                items: [
                  {
                    text: "数据结构",
                    collapsed: true,
                    items: [
                      {
                        text: "1. ADT",
                        link: "/教程/正文/数据结构与算法/数据结构/1_ADT",
                      },
                      {
                        text: "2. 线性表",
                        link: "/教程/正文/数据结构与算法/数据结构/2_线性表",
                      },
                    ],
                  },
                  {
                    text: "算法",
                    collapsed: true,
                    items: [
                      {
                        text: "1. 什么是算法",
                        link: "/教程/正文/数据结构与算法/算法/1_什么是算法",
                      },
                      {
                        text: "3. 算法的时间和空间复杂度",
                        link: "/教程/正文/数据结构与算法/算法/3_算法的时间和空间复杂度",
                      },
                      {
                        text: "4. 分治法和递归式",
                        link: "/教程/正文/数据结构与算法/算法/4_分治法和递归式",
                      },
                    ],
                  },
                ],
              },
              {
                text: "项目",
                collapsed: false,
                items: [
                  {
                    text: "项目结构",
                    link: "/教程/正文/项目/1_项目结构",
                  },
                  {
                    text: "Mdr Server",
                    collapsed: true,
                    items: [
                      {
                        text: "1. 项目概述 - Mdr Server",
                        link: "/教程/正文/项目/Mdr_Server/项目概述",
                      },
                    ],
                  },
                  {
                    text: "Mdr Chat Room",
                    collapsed: true,
                    items: [
                      {
                        text: "1. 项目概述 - Mdr Chat Room",
                        link: "/教程/正文/项目/Mdr_Chat_Room/项目概述",
                      },
                    ],
                  },
                  {
                    text: "Mdr Lang",
                    collapsed: true,
                    items: [
                      {
                        text: "1. 项目概述 - Mdr Lang",
                        link: "/教程/正文/项目/Mdr_Lang/项目概述",
                      },
                    ],
                  },
                  {
                    text: "MdrDB",
                    collapsed: true,
                    items: [
                      {
                        text: "1. 项目概述 - MdrDB",
                        link: "/教程/正文/项目/MdrDB/项目概述",
                      },
                    ],
                  },
                  {
                    text: "MdrOS",
                    collapsed: true,
                    items: [
                      {
                        text: "1. 项目概述 - MdrOS",
                        link: "/教程/正文/项目/MdrOS/项目概述",
                      },
                    ],
                  },
                  {
                    text: "6. MdrHAL",
                    collapsed: true,
                    items: [],
                  },
                ],
              },
              {
                text: "嵌入式",
                collapsed: true,
              },
            ],
          },
        ],
        "/教程/番外": [
          {
            text: "番外目录",
          },
        ],
        "/杂项": [
          {
            text: "杂项目录",
          },
        ],
        "/教程/题解": [
          {
            text: "习题答案及解析",
            items: [
              {
                text: "快速导航",
                link: "/教程/题解/快速导航",
              },
              {
                text: "[1] 语法和标准库",
                collapsed: false,
                items: [
                  {
                    text: "[101] 在开始编程之前",
                    collapsed: true,
                    items: [
                      {
                        text: "10101",
                        link: "/教程/题解/语法和标准库/在开始编程之前/10101",
                      },
                      {
                        text: "10102",
                        link: "/教程/题解/语法和标准库/在开始编程之前/10102",
                      }
                    ],
                  },
                  {
                    text: "[110] 结构体",
                    collapsed: true,
                    items: [
                      {
                        text: "11001",
                        link: "/教程/题解/语法和标准库/结构体/11001"
                      },
                      {
                        text: "11002",
                        link: "/教程/题解/语法和标准库/结构体/11002"
                      },
                    ],
                  }
                ],
              },
            ],
          },
        ],
      },

      socialLinks: [
        {
          icon: "github",
          link: "https://github.com/Mdr-C-Tutorial/C",
        },
      ],
    },
    markdown: {
      lineNumbers: true,
      config: (md) => {
        md.use(mathjax3);
      },
    },
    vue: {
      template: {
        compilerOptions: {
          isCustomElement: (tag) => customElements.includes(tag),
        },
      },
    },
    sitemap: {
      hostname: "https://Mdr-C-Tutorial.github.io/C/"
    },
  }),
);
