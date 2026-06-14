import type { MarkdownIt } from "./types";
import { terminalContainer } from "./terminalContainer";
import { compareContainer } from "./compareContainer";

/**
 * Register all custom markdown containers (`::: terminal`, `::: compare`)
 * onto a markdown-it instance. Intended for VitePress's `markdown.config` hook.
 */
export function setupContainers(md: MarkdownIt): void {
  terminalContainer(md);
  compareContainer(md);
}
