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
  description: "Mdr C Tutorial Repository",
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: "Home", link: "/" },
      { text: "总纲", link: "/教程/正文/总纲" },
      { text: "关于", link: "/About" },
    ],

    sidebar: {
      "/教程/正文/": [
        {
          text: "教程目录",
          items: [
            { text: "总纲", link: "/教程/正文/总纲" },
            {
              text: "语法和标准库",
              items: [
                {
                  text: "1. 在开始编程之前",
                  link: "/教程/正文/语法和标准库/1_在开始编程之前",
                },
                {
                  text: "2. 初识 C",
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
                  ],
                },
              ],
            },
            {
              text: "数据结构与算法",
              items: [
                {
                  text: "数据结构",
                  items: [
                    {
                      text: "1. ADT",
                      link: "/教程/正文/数据结构与算法/数据结构/1_ADT",
                    },
                  ],
                },
                {
                  text: "算法",
                  items: [
                    {
                      text: "1.什么是算法",
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
              items: [
                {
                  text: "1. Mdr Server",
                  items: [
                    {
                      text: "1.1 项目概述",
                      link: "/教程/正文/项目/Mdr_Server/项目概述",
                    },
                  ],
                },
              ],
            },
          ],
        },
      ],
      "/教程/番外": [{ text: "番外目录" }],
    },

    socialLinks: [
      { icon: "github", link: "https://github.com/Mdr-C-Tutorial/C" },
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
