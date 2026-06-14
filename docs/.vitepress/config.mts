import { defineConfig } from "vitepress";
import { withMermaid } from "vitepress-plugin-mermaid";
import { withPwa } from "@vite-pwa/vitepress";
import { readdirSync, readFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { Sidebar, Nav } from "./nscfg.mts";
import { setupContainers } from "./markdown/index";

const customElements: string[] = [""];
const bnfLanguage = {
  name: "bnf",
  displayName: "BNF",
  scopeName: "source.bnf",
  aliases: ["ebnf"],
  patterns: [
    {
      match: "^([A-Za-z][A-Za-z0-9_-]*)(:|::=)",
      captures: {
        1: { name: "entity.name.type.bnf" },
        2: { name: "keyword.operator.definition.bnf" },
      },
    },
    {
      match: "\\b[A-Za-z][A-Za-z0-9_-]*(?=\\s*(?::|::=))",
      name: "entity.name.type.bnf",
    },
    {
      match: "\\b[A-Za-z][A-Za-z0-9_-]*opt\\b",
      name: "storage.modifier.optional.bnf",
    },
    {
      match: "(::=|\\||\\.\\.\\.)",
      name: "keyword.operator.bnf",
    },
    {
      match: "[()\\[\\]{},:]",
      name: "punctuation.definition.bnf",
    },
  ],
};
const plainTextLanguages = ["in", "out"].map((name) => ({
  name,
  displayName: name.toUpperCase(),
  scopeName: `text.${name}`,
  patterns: [],
}));
const docsDir = join(dirname(fileURLToPath(import.meta.url)), "..");
const exerciseSourcesById = new Map<string, { href: string; title: string }[]>();

function docsLinkFromPath(src: string) {
  return `/${src.replace(/\\/g, "/").replace(/\.md$/, "")}`;
}

function pageTitleFromMarkdown(path: string, content: string) {
  return /^#\s+(.+)$/m.exec(content)?.[1] ?? path.split(/[\\/]/).at(-1) ?? path;
}

function collectExerciseSources(dir: string) {
  for (const item of readdirSync(dir, { withFileTypes: true })) {
    const file = join(dir, item.name);
    if (item.isDirectory()) {
      collectExerciseSources(file);
      continue;
    }

    if (!item.isFile() || !item.name.endsWith(".md")) continue;

    const content = readFileSync(file, "utf8");
    const href = docsLinkFromPath(relative(docsDir, file));
    const title = pageTitleFromMarkdown(file, content);

    for (const match of content.matchAll(/<Exercise\b[^>]*\bid="([^"]+)"[^>]*>/g)) {
      const id = match[1];
      const sources = exerciseSourcesById.get(id) ?? [];
      if (!sources.some((source) => source.href === href)) {
        sources.push({ href, title });
      }
      exerciseSourcesById.set(id, sources);
    }
  }
}

collectExerciseSources(join(docsDir, "教程", "正文"));

// https://vitepress.dev/reference/site-config
export default withPwa(
  withMermaid(
    defineConfig({
      title: "Mdr-C-Tutorial",
      titleTemplate: "Mdr-C-Tutorial",
      description: "Mdr C Tutorial 讲义仓库",
      lang: "zh-CN",
      base: "/C/",
      cleanUrls: true,
      metaChunk: true,
      lastUpdated: true,
      themeConfig: {
        nav: Nav,
        sidebar: Sidebar,
        outline: {
          level: [2, 4],
          label: "页面导航",
        },

        socialLinks: [
          {
            icon: "github",
            link: "https://github.com/Mdr-C-Tutorial/C",
          },
        ],
        editLink: {
          pattern: "https://github.com/Mdr-C-Tutorial/C/edit/main/docs/:path",
          text: "编辑此页",
        },
        lastUpdated: {
          text: "最后更新于",
          formatOptions: {
            dateStyle: "full",
            timeStyle: "full",
          },
        },
        search: {
          provider: "local",
          options: {
            translations: {
              button: {
                buttonText: "搜索文档",
                buttonAriaLabel: "搜索文档",
              },
              modal: {
                noResultsText: "无法找到相关结果",
                resetButtonTitle: "清除查询条件",
                footer: {
                  selectText: "选择",
                  navigateText: "切换",
                },
              },
            },
          },
        },
        docFooter: {
          prev: "上一页",
          next: "下一页",
        },
        darkModeSwitchLabel: "主题",
        lightModeSwitchTitle: "切换至浅色",
        darkModeSwitchTitle: "切换至深色",

        sidebarMenuLabel: "菜单",
        returnToTopLabel: "返回顶部",
        externalLinkIcon: true,
      },
      markdown: {
        lineNumbers: true,
        languages: [bnfLanguage, ...plainTextLanguages],
        codeCopyButtonTitle: "复制代码",
        math: true,
        image: {
          lazyLoading: true,
        },
        config(md) {
          setupContainers(md);
        },
      },
      vue: {
        customElement: customElements,
      },
      sitemap: {
        hostname: "https://mdr.aymao.com/C/",
      },
      transformPageData(pageData) {
        const id = /^教程\/题解\/.*\/(\d+)\.md$/.exec(
          pageData.relativePath,
        )?.[1];
        if (!id) return;

        pageData.frontmatter.exerciseSources = exerciseSourcesById.get(id) ?? [];
      },
      pwa: {
        strategies: "generateSW",
        registerType: "autoUpdate",
        injectRegister: "script-defer",
        includeAssets: ["favicon.svg", "favicon.ico"],
        pwaAssets: {},
        workbox: {
          globPatterns: ["**/*.{css,js,woff2}"],
          maximumFileSizeToCacheInBytes: 10485760,
        },
        experimental: {
          includeAllowlist: true,
        },
      },
    }),
  ),
);
