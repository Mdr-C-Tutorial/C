import container from "markdown-it-container";
import type { MarkdownIt, MarkdownItToken } from "./types";

/**
 * Parse `name="value"` style attributes from a container info string.
 * Returns the unescaped value, or `null` if the attribute is absent.
 */
function readAttr(info: string, name: string): string | null {
  const match = new RegExp(`${name}\\s*=\\s*"([^"]*)"`).exec(info);
  return match ? match[1] : null;
}

/**
 * Registers the `::: terminal` markdown container.
 *
 * ```md
 * ::: terminal title="zsh - my_program"
 * <span class="prompt">$</span> gcc main.c
 *
 * Hello, world!
 * <span class="prompt">$</span>
 * :::
 * ```
 *
 * Renders as `<TerminalWindow title="…">…</TerminalWindow>`.
 */
export function terminalContainer(md: MarkdownIt): void {
  md.use(container, "terminal", {
    validate(params: string) {
      return /^terminal(?:\s|$)/.test(params.trim());
    },
    render(tokens: MarkdownItToken[], idx: number) {
      const token = tokens[idx];
      if (token.nesting === 1) {
        const info = token.info.replace(/^terminal/, "").trim();
        const title = readAttr(info, "title") ?? "";
        const titleAttr = title
          ? ` title="${md.utils.escapeHtml(title)}"`
          : "";
        return `<TerminalWindow${titleAttr}>\n`;
      }
      return "</TerminalWindow>\n";
    },
  });
}
