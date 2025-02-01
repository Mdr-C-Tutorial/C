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
                        text: "3.2 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                        collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.6 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释", collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                    ]
                  },
                  {
                    text: "4. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释", collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释", collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释", collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "5. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释", collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释", collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释", collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                    ],
                  },
                  {
                    text: "6. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "7. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "8. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序"
                  },
                  {
                    text: "9. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释", collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释", collapsed: false,
                        items: [
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                          {
                            text: "3.1 注释",
                            link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                          },
                        ],
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "10. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "11. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序"
                  },
                  {
                    text: "12. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "13. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      }, {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "14. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      }, {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "15. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序"
                  },
                  {
                    text: "16. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "17. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "18. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序"
                  },
                  {
                    text: "19. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序"
                  },
                  {
                    text: "20. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序"
                  },
                  {
                    text: "21. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "22. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序",
                    collapsed: true,
                    items: [
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                      {
                        text: "3.1 注释",
                        link: "/教程/正文/语法和标准库/3_基本概念/3_1_注释",
                      },
                    ],
                  },
                  {
                    text: "23. 一个简单的 C 程序",
                    link: "/教程/正文/语法和标准库/2_一个简单的C程序"
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
