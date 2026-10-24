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
