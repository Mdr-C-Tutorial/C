/** @type {import("prettier").Config} */
const config = {
  semi: true,
  singleQuote: false,
  trailingComma: "all",
  printWidth: 88,
  tabWidth: 2,
  useTabs: false,
  bracketSpacing: true,
  bracketSameLine: false,
  singleAttributePerLine: true,
  htmlWhitespaceSensitivity: "css",
  vueIndentScriptAndStyle: false,
  endOfLine: "lf",
  overrides: [
    {
      files: "*.md",
      options: {
        proseWrap: "preserve",
      },
    },
  ],
};

export default config;
