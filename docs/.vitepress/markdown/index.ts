import type { MarkdownIt } from "./types.ts";
import footnote from "markdown-it-footnote";
import { terminalContainer } from "./terminalContainer.ts";
import { compareContainer } from "./compareContainer.ts";

/**
 * Register all custom markdown containers (`::: terminal`, `::: compare`)
 * onto a markdown-it instance. Intended for VitePress's `markdown.config` hook.
 */
export function setupContainers(md: MarkdownIt): void {
  terminalContainer(md);
  compareContainer(md);
}

/** Register standard Markdown footnote syntax used by the documentation. */
export function setupFootnotes(md: MarkdownIt): void {
  md.use(footnote);
}

/**
 * Add a "Run in Mainly.C" button to C and C++ code blocks.
 */
export function setupRunButton(md: MarkdownIt): void {
  const originalFence = md.renderer.rules.fence;
  if (!originalFence) return;
  md.renderer.rules.fence = (...args) => {
    const html = originalFence(...args);
    if (
      /\blanguage-(c|cpp|cxx|cc)\b/.test(html) &&
      !/\blanguage-(cmake|css)\b/.test(html)
    ) {
      const runButtonHtml =
        '<button title="在 Mainly.C 中运行" class="run" aria-label="在 Mainly.C 中运行"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="16" height="16" fill="currentColor" style="margin-left: 2px;"><polygon points="6 3 20 12 6 21 6 3"></polygon></svg></button>';
      return html.replace(
        /class="copy"><\/button>/,
        `class="copy"></button>${runButtonHtml}`,
      );
    }
    return html;
  };
}
