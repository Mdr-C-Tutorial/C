import { tokenize } from "./lexer.mjs";
import { parse } from "./parser.mjs";

/**
 * @param {string} source
 * @param {string} filePath
 */
export function compileSource(source, filePath) {
  const tokens = tokenize(source, filePath);
  const ast = parse(tokens, filePath);
  return { tokens, ast };
}
