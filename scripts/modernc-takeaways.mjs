// 从《Modern C》译稿各章正文抽取“要点”框，生成 docs/教程/ModernC/要点汇总.md。
// 修改要点时只改正文，然后运行 `pnpm modernc:takeaways`；加 `--check` 只检查汇总是否最新。

import { readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

const DIR = fileURLToPath(new URL("../docs/教程/ModernC/", import.meta.url));
const OUT = "要点汇总.md";
const SOURCES = [
  "前言",
  "01_入门",
  "02_程序的基本结构",
  "03_一切都与控制有关",
  "04_表达计算",
  "05_基本值与数据",
  "06_派生数据类型",
  "07_函数",
  "08_C库函数",
  "09_风格",
  "10_组织与文档",
  "11_指针",
  "12_C内存模型",
  "13_存储",
  "14_进阶处理与输入输出",
  "15_程序故障",
  "16_性能",
  "17_类函数宏",
  "18_类型泛型编程",
  "19_控制流的变化",
  "20_线程",
  "21_原子访问与内存一致性",
  "技术附录",
];

// 与 VitePress 默认的标题锚点算法保持一致
const slugify = (str) =>
  str
    .normalize("NFKD")
    .replace(/[̀-ͯ]/g, "")
    // eslint-disable-next-line no-control-regex -- 照搬 VitePress，去除控制字符
    .replace(/[\u0000-\u001f]/g, "")
    .replace(/[\s~`!@#$%^&*()\-_+=[\]{}|\\;:"'“”‘’<>,.?/]+/g, "-")
    .replace(/-{2,}/g, "-")
    .replace(/^-+|-+$/g, "")
    .replace(/^(\d)/, "_$1")
    .toLowerCase();

// 标题渲染后的纯文本：去掉行内代码与强调标记、HTML 标签，还原实体
const headingText = (src) =>
  src
    .replace(/`([^`]*)`/g, "$1")
    .replace(/\*\*|__/g, "")
    .replace(/<[^>]+>/g, "")
    .replace(/&lt;/g, "<")
    .replace(/&gt;/g, ">")
    .replace(/&amp;/g, "&")
    .trim();

function extract(name) {
  const lines = readFileSync(`${DIR}${name}.md`, "utf8").split("\n");
  const title = lines[0].replace(/^#\s*/, "");
  const seen = new Map();
  const takeaways = [];
  let anchor = "";
  let fence = null;

  for (let i = 1; i < lines.length; i++) {
    const line = lines[i];
    const f = line.trimStart().match(/^(`{3,}|~{3,})/);
    if (f && (!fence || f[1].startsWith(fence))) {
      fence = fence ? null : f[1];
      continue;
    }
    if (fence) continue;

    const h = line.match(/^#{2,6}\s+(.*)$/);
    if (h) {
      const slug = slugify(headingText(h[1]));
      const n = seen.get(slug) ?? 0;
      seen.set(slug, n + 1);
      anchor = n ? `${slug}-${n}` : slug;
      continue;
    }

    const box = line.match(/^:::\s*\w+\s+(要点.*)$/);
    if (box) {
      const body = [];
      while (lines[++i].trim() !== ":::") body.push(lines[i]);
      takeaways.push({ label: box[1].trim(), anchor, body: body.join("\n").trim() });
    }
  }
  return { name, title, takeaways };
}

function render(chapters) {
  const out = [
    "# 要点汇总",
    "",
    "<!-- 本文件由 scripts/modernc-takeaways.mjs 从各章正文生成，请勿手工编辑：修改要点请改正文，再运行 pnpm modernc:takeaways。 -->",
    "",
    "本页汇集全书各章的要点，可作速查表使用。点击要点编号，可跳回正文中该要点所在的小节。",
    "",
    "[[toc]]",
  ];
  for (const { name, title, takeaways } of chapters) {
    if (!takeaways.length) continue;
    out.push("", `## ${title}`);
    for (const { label, anchor, body } of takeaways) {
      const link = `/教程/ModernC/${name}${anchor ? `#${anchor}` : ""}`;
      out.push("", `::: tip [${label}](${link})`, body, ":::");
    }
  }
  return out.join("\n") + "\n";
}

const result = render(SOURCES.map(extract));
const path = `${DIR}${OUT}`;

if (process.argv.includes("--check")) {
  if (readFileSync(path, "utf8") !== result) {
    console.error(`${OUT} 已过期，请运行 pnpm modernc:takeaways`);
    process.exit(1);
  }
  console.log(`${OUT} 已是最新`);
} else {
  writeFileSync(path, result);
  const count = (result.match(/^::: tip /gm) ?? []).length;
  console.log(`已生成 ${OUT}：${count} 条要点`);
}
