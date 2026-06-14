# CLAUDE.md

本文件记录对本项目工作至关重要的约定与陷阱。通用写作规范见 `AGENTS.md` 与 `README.md`，此处不重复。

## 工作约定

- **不要自行启动服务**：`pnpm docs:dev` 等长驻进程由用户按需启动，不要代为启动。本地验证 markdown 渲染用 `vite-node` + `markdown-it` 单测，而非起 dev server。
- **Ultracode / Workflow 编排是可选的**：仅当用户在本会话明确 opt-in 时才用 `Workflow` 工具。不确定就直接做或先问。
- 章节编号变动（重排、增删章/节）时，必须同步：文件名、目录名、`nscfg.mts` 侧边栏、父章与节索引文件的目录链接、以及全仓库对该路径的交叉引用。`git grep` 验证无旧路径残留后再算完成。

## 自定义 Markdown 容器

`docs/.vitepress/markdown/` 下注册了两个容器，包装 Vue 组件（见 `config.mts` 的 `markdown.config`）。优先用容器语法，而不是直接写组件标签。

### `::: terminal`（包装 `<TerminalWindow>`）

```markdown
::: terminal title="zsh - my_program"

<span class="prompt">$</span> gcc main.c

Hello, world!

:::
```

- 标题可省略（无标题时写 `::: terminal`）。
- 容器支持列表项内缩进。

### `::: compare`（包装 `<CodeCompare>`）

```markdown
::: compare left-title="错误写法" right-title="正确写法"

```c
int *p;
```

---

```c
int *p = &x;
```

:::
```

- 用 `---`（thematic break）分隔左右两栏。
- 可选 `vertical` 标志改为上下布局；`left-title` / `right-title` 可省略（用组件默认值）。
- 仅把 `<CodeCompare>` 改为容器语法；`<Quiz>` / `<Exercise>` / `<Term>` 等组件保持原写法不变（它们不适合容器化）。

## 依赖

- `markdown-it-container`（dependencies）：容器插件依赖。
- `markdown-it`（devDependencies）：仅用于本地 markdown 渲染测试，非构建必需。

## 文件陷阱

- 部分 `.md` 文件开头残留 UTF-8 BOM（如历史版本的 `4_2_11_运算符宏.md`）。编辑时如发现 `#` 标题前有不可见字符，顺手用 `sed -i '1s/^\xEF\xBB\xBF//'` 清除。
