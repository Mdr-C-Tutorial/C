import { defineCourse } from "@gentorial/core";

export default defineCourse({
  schemaVersion: "1",
  id: "mdr-c-tutorial",
  title: "Mdr C Tutorial",
  description:
    "涵盖语法和标准库、开发与工程基础、数据结构与算法、系统编程、项目案例和嵌入式的生成式 C 语言教程。",
  lang: "zh-CN",
  contentDir: "docs",
  generation: {
    mode: "hybrid",
    defaultLocale: "zh-CN",
  },
  rendering: {
    allowUnsafeHtml: false,
  },
  accuracy: {
    standards: ["ISO C11", "ISO C17", "ISO C23"],
    policies: [
      "不得混淆不同 C 标准版本引入的语法和标准库能力",
      "使用 C11 之后引入的能力时必须明确标注最低标准版本",
      "不得把编译器扩展描述为 ISO C 标准能力",
      "代码示例必须说明编译前提；未经验证的内容只能作为个性化讲解",
    ],
  },
});
