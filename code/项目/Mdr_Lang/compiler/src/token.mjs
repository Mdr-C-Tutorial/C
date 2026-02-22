export const TOKEN_KIND = {
  KEYWORD: "keyword",
  IDENTIFIER: "identifier",
  NUMBER: "number",
  STRING: "string",
  CHAR: "char",
  OPERATOR: "operator",
  EOF: "eof",
};

export const KEYWORDS = new Set([
  "use",
  "function",
  "let",
  "const",
  "struct",
  "if",
  "else",
  "while",
  "loop",
  "return",
  "break",
  "continue",
  "for",
  "true",
  "false",
  "this",
  "null",
]);

export const TYPE_KEYWORDS = new Set([
  "i8",
  "i16",
  "i32",
  "i64",
  "u8",
  "u16",
  "u32",
  "u64",
  "f32",
  "f64",
  "bool",
  "char",
]);

export const OPERATORS = [
  "...",
  "..",
  "->",
  "||",
  "&&",
  "==",
  "!=",
  ">=",
  "<=",
  "+=",
  "-=",
  "*=",
  "/=",
  "=",
  ">",
  "<",
  "+",
  "-",
  "*",
  "/",
  "!",
  ".",
  ",",
  ":",
  ";",
  "(",
  ")",
  "{",
  "}",
  "[",
  "]",
];

/**
 * @typedef {object} Token
 * @property {string} kind
 * @property {string} lexeme
 * @property {number} offset
 * @property {number} line
 * @property {number} column
 * @property {number} endOffset
 * @property {number} endLine
 * @property {number} endColumn
 */
