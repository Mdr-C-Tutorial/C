import { defineConfig } from "vitepress";
import { withMermaid } from "vitepress-plugin-mermaid";
import { withPwa } from "@vite-pwa/vitepress";
import { Sidebar, Nav } from "./nscfg.mts";

const customElements: string[] = [""];

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
        codeCopyButtonTitle: "复制代码",
        math: true,
        image: {
          lazyLoading: true,
        },
      },
      vue: {
        customElement: customElements,
      },
      sitemap: {
        hostname: "https://mdr.aymao.com/C/",
      },
      pwa: {
        strategies: "generateSW",
        registerType: "autoUpdate",
        injectRegister: "script-defer",
        includeAssets: ["favicon.svg", "favicon.ico"],
        pwaAssets: {},
        workbox: {
          globPatterns: ["**/*.{css,js,html,woff2}"],
          maximumFileSizeToCacheInBytes: 10485760,
        },
        experimental: {
          includeAllowlist: true,
        },
      },
    }),
  ),
);
