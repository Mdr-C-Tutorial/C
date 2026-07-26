# Gentorial 接入说明

Mdr C Tutorial 在保留原有 VitePress 教程、组件、习题与导航的基础上接入 Gentorial。作者内容定义事实、范围、代码和准确性边界；`generate` 区域把这些材料组织成适合当前学习者的主要个性化讲解。没有生成服务时，作者原文仍可直接阅读。

## 当前版本

| 包 | 版本 |
| --- | --- |
| `@gentorial/ai` | `^0.3.0` |
| `@gentorial/core` | `^0.1.1` |
| `@gentorial/engine-vitepress` | `^0.1.0` |
| `@gentorial/runtime-vue` | `^0.3.0` |
| `@gentorial/theme-default` | `^0.2.0` |

依赖全部来自 npm，不依赖本机相邻仓库。

## 接入边界

- VitePress、PWA、脚注、数学公式和原有自定义容器继续由本仓库配置。
- `gentorialMarkdown` 在脚注与原有容器之后接入现有 Markdown 管线。
- 作者与 AI 生成的 `mermaid` 围栏由 Gentorial 默认主题渲染。
- 原主题继续注册题解、终端、练习、测验、代码运行器和 Service Worker 组件。
- Mdr C Tutorial 布局会转发 Gentorial 注入的 Preferences 导航插槽。
- 当前使用纯前端 BYOK；未配置时不生成 mock 内容，而是显示明确错误。
- API Key 默认只保存在当前页面内存中，不写入静态产物、`localStorage` 或日志。
- 运行时支持流式 Markdown、连续追问、分支学习路径和上下文限制。

## 正文约定

1. `concept` 保存标准版本、定义、边界、未定义行为与其他不可反转的结论。
2. `generate` 负责生成完整教学讲解，而不是只补充一小段 AI 内容。
3. 每个 `generate` 必须位于所属标题的作者正文末尾，后面直接进入下一个标题。
4. 可编译的基准代码、确定性习题、测验和参考答案继续由作者维护。
5. 标准文档翻译保持静态；生成内容只能作为阅读辅助，不能改写译文。

验证页位于 [`docs/gentorial-integration.md`](./docs/gentorial-integration.md)。

## 验证

```bash
pnpm install --frozen-lockfile
pnpm exec tsc --noEmit
pnpm docs:build
pnpm mdr:test
```

完整构建会让全部 Markdown 页面经过 Gentorial 内容解析并生成 PWA 产物。

## 内容质量边界

课程准确性策略会进入模型提示，但 Gentorial 不替代事实审核。新增生成区域时，仍需标明 C 标准前提，并为可独立编译的代码建立 GCC/Clang 检查；无法独立编译的片段应提供包装模板或显式跳过标记。
