import { defineConfig } from "vitepress";
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
export default defineConfig({
  title: "Mdr-C-Tutorial",
  titleTemplate: "Mdr-C-Tutorial",
  description: "Mdr C Tutorial Repository",
  lang: "zh-CN",
  base: "/C/",
  cleanUrls: true,
  metaChunk: true,
  lastUpdated: true,
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
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
                  text: "2. 初识 C",
                  collapsed: true,
                  items: [
                    {
                      text: "2.1 一个简单的 C 程序",
                      link: "/教程/正文/语法和标准库/2_初识C/2_1_一个简单的C程序",
                    },
                    {
                      text: "2.2 注释",
                      link: "/教程/正文/语法和标准库/2_初识C/2_2_注释",
                    },
                    {
                      text: "2.3 对象",
                      link: "/教程/正文/语法和标准库/2_初识C/2_3_C对象_初步",
                    },
                    {
                      text: "2.4 数据",
                      link: "/教程/正文/语法和标准库/2_初识C/2_4_数据",
                    },
                    {
                      text: "2.5 字符串字面量",
                      link: "/教程/正文/语法和标准库/2_初识C/2_5_字符串字面量",
                    },
                    {
                      text: "2.6 断言",
                      link: "/教程/正文/语法和标准库/2_初识C/2_6_初识断言",
                    },
                  ],
                },
                {
                  text: "3. 基本概念",
                  collapsed: true,
                  items: [
                    {
                      text: "3.1 行为",
                      link: "/教程/正文/语法和标准库/3_基本概念/3_1_行为",
                    },
                    {
                      text: "3.2 翻译单元",
                      link: "/教程/正文/语法和标准库/3_基本概念/3_2_翻译单元",
                    },
                    {
                      text: "3.3 翻译阶段",
                      link: "/教程/正文/语法和标准库/3_基本概念/3_3_翻译阶段",
                    },
                  ],
                },
                {
                  text: "4. 表达式",
                  link: "/教程/正文/语法和标准库/4_表达式",
                  collapsed: true,
                  items: [
                    {
                      text: "4.1 运算符",
                      link: "/教程/正文/语法和标准库/4_表达式/4_1_运算符",
                    },
                    {
                      text: "4.2 求值顺序",
                      link: "/教程/正文/语法和标准库/4_表达式/4_2_求值顺序",
                    },
                    {
                      text: "4.3 整数常量 浮点常量",
                      link: "/教程/正文/语法和标准库/4_表达式/4_3_整数常量_浮点常量",
                    },
                    {
                      text: "4.4 字符常量",
                      link: "/教程/正文/语法和标准库/4_表达式/4_4_字符常量",
                    },
                    {
                      text: "4.5 隐式转换",
                      link: "/教程/正文/语法和标准库/4_表达式/4_5_隐式转换",
                    },
                    {
                      text: "4.6 值类别",
                      link: "/教程/正文/语法和标准库/4_表达式/4_6_值类别",
                    },
                  ],
                },
                {
                  text: "5. 语句",
                  link: "/教程/正文/语法和标准库/5_语句",
                },
                {
                  text: "6. 函数",
                  link: "/教程/正文/语法和标准库/6_函数",
                  collapsed: true,
                  items: [
                    {
                      text: "6.1 函数声明和定义",
                      link: "/教程/正文/语法和标准库/6_函数/6_1_函数声明和定义",
                    },
                    {
                      text: "6.2 函数调用",
                      link: "/教程/正文/语法和标准库/6_函数/6_2_函数调用",
                    },
                    {
                      text: "6.3 递归和迭代",
                      link: "/教程/正文/语法和标准库/6_函数/6_3_递归和迭代",
                    },
                  ],
                },
                {
                  text: "7. 数组",
                  link: "/教程/正文/语法和标准库/7_数组",
                },
                {
                  text: "8. 指针",
                  link: "/教程/正文/语法和标准库/8_指针",
                  collapsed: true,
                  items: [
                    {
                      text: "8.1 restrict",
                      link: "/教程/正文/语法和标准库/8_指针/8_1_restrict",
                    },
                  ],
                },
                {
                  text: "9. 声明",
                  link: "/教程/正文/语法和标准库/9_声明",
                  collapsed: true,
                  items: [
                    {
                      text: "9.1 const volatile",
                      link: "/教程/正文/语法和标准库/9_声明/9_1_const_volatile",
                    },
                  ],
                },
                {
                  text: "10. 结构体",
                  link: "/教程/正文/语法和标准库/10_结构体",
                  collapsed: true,
                  items: [
                    {
                      text: "10.1 结构体对齐",
                      link: "/教程/正文/语法和标准库/10_结构体/10_1_结构体对齐",
                    },
                    {
                      text: "10.2 柔性数组成员",
                      link: "/教程/正文/语法和标准库/10_结构体/10_2_柔性数组",
                    },
                    {
                      text: "10.3 位域",
                      link: "/教程/正文/语法和标准库/10_结构体/10_3_位域",
                    },
                  ],
                },
                {
                  text: "11. 联合体",
                  link: "/教程/正文/语法和标准库/11_联合体",
                },
                {
                  text: "12. 对象",
                  collapsed: true,
                  items: [
                    {
                      text: "12.1 对象表示",
                      link: "/教程/正文/语法和标准库/12_对象/12_1_对象表示",
                      collapsed: false,
                      items: [
                        {
                          text: "12.1.1 内存对齐",
                          link: "/教程/正文/语法和标准库/12_对象/12_1_对象表示/12_1_1_内存对齐.md",
                        },
                        {
                          text: "12.1.2 alignas",
                          link: "/教程/正文/语法和标准库/12_对象/12_1_对象表示/12_1_2_alignas.md",
                        },
                      ],
                    },
                    {
                      text: "12.2 生存期",
                      link: "/教程/正文/语法和标准库/12_对象/12_2_生存期",
                    },
                  ],
                },
                {
                  text: "13. 枚举",
                  link: "/教程/正文/语法和标准库/13_枚举",
                },
                {
                  text: "14. 查找 命名空间",
                  link: "/教程/正文/语法和标准库/14_查找_命名空间",
                },
                {
                  text: "15. 内存管理",
                  link: "/教程/正文/语法和标准库/15_内存管理",
                },
                {
                  text: "16. 文件操作",
                  link: "/教程/正文/语法和标准库/16_文件操作",
                },
                {
                  text: "17. 可变参数",
                  link: "/教程/正文/语法和标准库/17_可变参数",
                },
                {
                  text: "18. 泛型选择",
                  link: "/教程/正文/语法和标准库/18_泛型选择",
                },
                {
                  text: "19. 属性",
                  link: "/教程/正文/语法和标准库/19_属性",
                },
                {
                  text: "20. 错误处理",
                  link: "/教程/正文/语法和标准库/20_错误处理",
                },
                {
                  text: "21. 线程",
                  link: "/教程/正文/语法和标准库/21_线程",
                },
                {
                  text: "22. 原子",
                  link: "/教程/正文/语法和标准库/22_原子",
                },
                {
                  text: "23. 预处理器",
                  link: "/教程/正文/语法和标准库/23_预处理器",
                },
                {
                  text: "24. 标准库",
                  collapsed: true,
                  items: [
                    {
                      text: "24.1 标准库介绍",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_1_C标准库的介绍和文档",
                    },
                    {
                      text: "24.2 独立 宿主实现",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_2_freestanding_宿主实现",
                    },
                    {
                      text: "24.3 预定义宏",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_3_C预定义宏",
                    },
                    {
                      text: "24.4 输入输出库",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_4_C输入输出库",
                    },
                    {
                      text: "24.5 数学库",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_5_C数学库",
                    },
                    {
                      text: "24.6 复数库",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_6_C复数库",
                    },
                    {
                      text: "24.7 二分查找 快速排序",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_7_二分查找与快排",
                    },
                    {
                      text: "24.8 随机数生成",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_8_随机数生成",
                    },
                    {
                      text: "24.9 时间日期库",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_9_C时间日期库",
                    },
                    {
                      text: "24.10 字符处理库",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_10_C字符处理库",
                    },
                    {
                      text: "24.11 字符串处理库",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_11_字符串API",
                    },
                    {
                      text: "24.12 执行控制函数",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_12_执行控制函数",
                    },
                    {
                      text: "24.13 标准库扩展",
                      link: "/教程/正文/语法和标准库/24_C标准库/24_13_标准库扩展",
                    },
                  ],
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
                    {
                      text: "2. 引言 - MdrOS",
                      link: "/教程/正文/项目/MdrOS/root",
                    },
                    {
                      text: "3. 引导程序 - MdrOS",
                      link: "/教程/正文/项目/MdrOS/bootloader",
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
        }
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
});
