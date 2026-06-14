declare module "markdown-it-container" {
  // Minimal ambient declaration — markdown-it-container ships no types.
  // The actual API is `md.use(container, name, options)`; we type it loosely
  // and rely on our own `MarkdownIt` structural type for the parts we use.
  const container: (
    md: unknown,
    name: string,
    options?: {
      validate?: (params: string) => boolean;
      render?: (...args: unknown[]) => string;
      marker?: string;
    },
  ) => unknown;
  export default container;
}
