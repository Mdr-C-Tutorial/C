import { MdrCompileError } from "./error.mjs";
import { KEYWORDS, OPERATORS, TOKEN_KIND } from "./token.mjs";

/**
 * @param {string} source
 * @param {string} filePath
 */
export function tokenize(source, filePath) {
  const lexer = new Lexer(source, filePath);
  return lexer.tokenize();
}

class Lexer {
  /**
   * @param {string} source
   * @param {string} filePath
   */
  constructor(source, filePath) {
    this.source = source;
    this.filePath = filePath;
    this.length = source.length;
    this.offset = 0;
    this.line = 1;
    this.column = 1;
    /** @type {import("./token.mjs").Token[]} */
    this.tokens = [];
  }

  tokenize() {
    while (!this.isAtEnd()) {
      this.skipWhitespaceAndComments();
      if (this.isAtEnd()) {
        break;
      }
      this.scanToken();
    }

    this.tokens.push({
      kind: TOKEN_KIND.EOF,
      lexeme: "",
      offset: this.offset,
      line: this.line,
      column: this.column,
      endOffset: this.offset,
      endLine: this.line,
      endColumn: this.column,
    });

    return this.tokens;
  }

  isAtEnd() {
    return this.offset >= this.length;
  }

  /**
   * @param {number} [n]
   */
  peek(n = 0) {
    const next = this.offset + n;
    if (next >= this.length) {
      return "\0";
    }
    return this.source[next];
  }

  advance() {
    const ch = this.source[this.offset];
    this.offset += 1;
    if (ch === "\n") {
      this.line += 1;
      this.column = 1;
    } else {
      this.column += 1;
    }
    return ch;
  }

  /**
   * @param {string} kind
   * @param {number} startOffset
   * @param {number} startLine
   * @param {number} startColumn
   */
  pushToken(kind, startOffset, startLine, startColumn) {
    this.tokens.push({
      kind,
      lexeme: this.source.slice(startOffset, this.offset),
      offset: startOffset,
      line: startLine,
      column: startColumn,
      endOffset: this.offset,
      endLine: this.line,
      endColumn: this.column,
    });
  }

  skipWhitespaceAndComments() {
    while (!this.isAtEnd()) {
      const ch = this.peek();
      if (ch === " " || ch === "\t" || ch === "\r" || ch === "\n") {
        this.advance();
        continue;
      }

      if (ch === "/" && this.peek(1) === "/") {
        this.advance();
        this.advance();
        while (!this.isAtEnd() && this.peek() !== "\n") {
          this.advance();
        }
        continue;
      }

      break;
    }
  }

  scanToken() {
    const startOffset = this.offset;
    const startLine = this.line;
    const startColumn = this.column;
    const ch = this.peek();

    if (isIdentifierStart(ch)) {
      this.scanIdentifier(startOffset, startLine, startColumn);
      return;
    }

    if (isDigit(ch)) {
      this.scanNumber(startOffset, startLine, startColumn);
      return;
    }

    if (ch === "\"") {
      this.scanString(startOffset, startLine, startColumn);
      return;
    }

    if (ch === "'") {
      this.scanChar(startOffset, startLine, startColumn);
      return;
    }

    for (const op of OPERATORS) {
      if (this.source.startsWith(op, this.offset)) {
        for (let i = 0; i < op.length; i += 1) {
          this.advance();
        }
        this.pushToken(TOKEN_KIND.OPERATOR, startOffset, startLine, startColumn);
        return;
      }
    }

    throw new MdrCompileError(`unexpected character: ${ch}`, {
      filePath: this.filePath,
      line: startLine,
      column: startColumn,
    });
  }

  scanIdentifier(startOffset, startLine, startColumn) {
    this.advance();
    while (isIdentifierPart(this.peek())) {
      this.advance();
    }
    const text = this.source.slice(startOffset, this.offset);
    const kind = KEYWORDS.has(text) ? TOKEN_KIND.KEYWORD : TOKEN_KIND.IDENTIFIER;
    this.pushToken(kind, startOffset, startLine, startColumn);
  }

  scanNumber(startOffset, startLine, startColumn) {
    this.advance();
    while (isDigit(this.peek())) {
      this.advance();
    }

    if (this.peek() === "." && this.peek(1) !== "." && isDigit(this.peek(1))) {
      this.advance();
      while (isDigit(this.peek())) {
        this.advance();
      }
    }

    if (this.peek() === "e" || this.peek() === "E") {
      const saveOffset = this.offset;
      const saveLine = this.line;
      const saveColumn = this.column;

      this.advance();
      if (this.peek() === "+" || this.peek() === "-") {
        this.advance();
      }

      if (!isDigit(this.peek())) {
        this.offset = saveOffset;
        this.line = saveLine;
        this.column = saveColumn;
      } else {
        while (isDigit(this.peek())) {
          this.advance();
        }
      }
    }

    while (isIdentifierPart(this.peek())) {
      this.advance();
    }

    this.pushToken(TOKEN_KIND.NUMBER, startOffset, startLine, startColumn);
  }

  scanString(startOffset, startLine, startColumn) {
    this.advance();
    while (!this.isAtEnd()) {
      const ch = this.peek();
      if (ch === "\"") {
        this.advance();
        this.pushToken(TOKEN_KIND.STRING, startOffset, startLine, startColumn);
        return;
      }
      if (ch === "\\") {
        this.advance();
        if (this.isAtEnd()) {
          break;
        }
        this.advance();
        continue;
      }
      if (ch === "\n") {
        throw new MdrCompileError("unterminated string literal", {
          filePath: this.filePath,
          line: startLine,
          column: startColumn,
        });
      }
      this.advance();
    }

    throw new MdrCompileError("unterminated string literal", {
      filePath: this.filePath,
      line: startLine,
      column: startColumn,
    });
  }

  scanChar(startOffset, startLine, startColumn) {
    this.advance();
    if (this.isAtEnd() || this.peek() === "\n") {
      throw new MdrCompileError("unterminated char literal", {
        filePath: this.filePath,
        line: startLine,
        column: startColumn,
      });
    }

    if (this.peek() === "\\") {
      this.advance();
      if (this.isAtEnd()) {
        throw new MdrCompileError("unterminated char literal", {
          filePath: this.filePath,
          line: startLine,
          column: startColumn,
        });
      }
      this.advance();
    } else {
      this.advance();
    }

    if (this.peek() !== "'") {
      throw new MdrCompileError("invalid char literal", {
        filePath: this.filePath,
        line: startLine,
        column: startColumn,
      });
    }

    this.advance();
    this.pushToken(TOKEN_KIND.CHAR, startOffset, startLine, startColumn);
  }
}

/**
 * @param {string} ch
 */
function isIdentifierStart(ch) {
  return (ch >= "a" && ch <= "z") || (ch >= "A" && ch <= "Z") || ch === "_";
}

/**
 * @param {string} ch
 */
function isIdentifierPart(ch) {
  return isIdentifierStart(ch) || isDigit(ch);
}

/**
 * @param {string} ch
 */
function isDigit(ch) {
  return ch >= "0" && ch <= "9";
}
