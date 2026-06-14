import container from "markdown-it-container";
import type { MarkdownIt, MarkdownItState, MarkdownItToken } from "./types";

/**
 * Parse `name="value"` style attributes from a container info string.
 */
function readAttr(info: string, name: string): string | null {
  const match = new RegExp(`${name}\\s*=\\s*"([^"]*)"`).exec(info);
  return match ? match[1] : null;
}

/**
 * Check whether `flag` appears as a standalone word in `info`,
 * ignoring text inside double-quoted attribute values.
 */
function hasFlag(info: string, flag: string): boolean {
  const stripped = info.replace(/"[^"]*"/g, "");
  return new RegExp(`(?:^|\\s)${flag}(?:\\s|$)`).test(stripped);
}

/**
 * Registers the `::: compare` markdown container.
 *
 * Syntax — use `---` (thematic break) to split left / right slot content:
 *
 * ```md
 * ::: compare left-title="错误写法" right-title="正确写法"
 * ```c
 * int *p;
 * *p = 42;
 * ```
 * ---
 * ```c
 * int x = 42;
 * int *p = &x;
 * *p = 42;
 * ```
 * :::
 * ```
 *
 * Optional `vertical` flag renders as stacked layout instead of side-by-side.
 *
 * The container is parsed by markdown-it-container, then a core rule rewrites
 * the token stream into `<CodeCompare>` with `<template #left>` /
 * `<template #right>` named slots.
 */
export function compareContainer(md: MarkdownIt): void {
  // 1. Register the container so markdown-it recognises `::: compare` blocks.
  //    The render output is irrelevant — the core rule below replaces it.
  md.use(container, "compare", {
    validate(params: string) {
      return /^compare(?:\s|$)/.test(params.trim());
    },
    render() {
      return "";
    },
  });

  // 2. Rewrite each compare block into <CodeCompare> with two named slots,
  //    splitting the inner content on the first thematic break (---).
  md.core.ruler.after("block", "compare_slots", (state: MarkdownItState) => {
    const tokens = state.tokens;

    // Collect block ranges bottom-up so earlier indices stay valid.
    const ranges: Array<{
      open: number;
      close: number;
      hr: number;
    }> = [];

    for (let i = 0; i < tokens.length; i += 1) {
      if (tokens[i].type !== "container_compare_open") continue;

      let depth = 0;
      let closeIdx = -1;
      let hrIdx = -1;

      for (let j = i + 1; j < tokens.length; j += 1) {
        const type = tokens[j].type;
        if (type === "container_compare_open") {
          depth += 1;
        } else if (type === "container_compare_close") {
          if (depth === 0) {
            closeIdx = j;
            break;
          }
          depth -= 1;
        } else if (type === "hr" && depth === 0 && hrIdx === -1) {
          hrIdx = j;
        }
      }

      if (closeIdx !== -1) {
        ranges.push({ open: i, close: closeIdx, hr: hrIdx });
      }
    }

    // Process from bottom to top so splices don't invalidate earlier indices.
    for (let k = ranges.length - 1; k >= 0; k -= 1) {
      const { open, close, hr } = ranges[k];
      const info = tokens[open].info.replace(/^compare/, "").trim();
      const leftTitleRaw = readAttr(info, "left-title") ?? readAttr(info, "left") ?? "";
      const rightTitleRaw = readAttr(info, "right-title") ?? readAttr(info, "right") ?? "";
      const vertical = hasFlag(info, "vertical");

      const leftTitle = md.utils.escapeHtml(leftTitleRaw);
      const rightTitle = md.utils.escapeHtml(rightTitleRaw);

      const attrs =
        (leftTitle ? ` left-title="${leftTitle}"` : "") +
        (rightTitle ? ` right-title="${rightTitle}"` : "") +
        (vertical ? " vertical" : "");

      // Mutate the open / hr / close tokens in-place into html_block so
      // the renderer outputs them verbatim. No new Token objects needed.
      tokens[open].type = "html_block";
      tokens[open].content = `<CodeCompare${attrs}>\n<template #left>\n`;
      tokens[open].hidden = false;
      tokens[open].children = null;

      if (hr !== -1) {
        tokens[hr].type = "html_block";
        tokens[hr].content = "</template>\n<template #right>\n";
        tokens[hr].hidden = false;
        tokens[hr].children = null;
      }

      tokens[close].type = "html_block";
      tokens[close].content =
        hr !== -1
          ? "</template>\n</CodeCompare>\n"
          : "</template>\n<template #right></template>\n</CodeCompare>\n";
      tokens[close].hidden = false;
      tokens[close].children = null;
    }
  });
}
