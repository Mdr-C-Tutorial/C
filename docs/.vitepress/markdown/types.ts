// Structural types for the markdown-it instance we touch.
// Kept local so these plugins don't depend on transitive @types (which pnpm's
// strict node_modules may not surface).

export interface MarkdownItToken {
  type: string;
  info: string;
  nesting: number;
  content: string;
  hidden: boolean;
  block: boolean;
  children: MarkdownItToken[] | null;
}

export interface MarkdownItState {
  tokens: MarkdownItToken[];
}

export interface MarkdownIt {
  use(plugin: unknown, ...args: unknown[]): unknown;
  utils: {
    escapeHtml(value: string): string;
  };
  core: {
    ruler: {
      after(
        afterName: string,
        ruleName: string,
        fn: (state: MarkdownItState) => void,
      ): void;
    };
  };
}
