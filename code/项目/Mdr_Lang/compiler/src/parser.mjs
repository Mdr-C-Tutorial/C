import { MdrCompileError } from "./error.mjs";
import { TOKEN_KIND, TYPE_KEYWORDS } from "./token.mjs";

const ASSIGNMENT_OPERATORS = new Set(["=", "+=", "-=", "*=", "/="]);
const UNARY_OPERATORS = new Set(["-", "!"]);
const TYPE_START_OPERATORS = new Set(["[", "("]);

/**
 * @param {import("./token.mjs").Token[]} tokens
 * @param {string} filePath
 */
export function parse(tokens, filePath) {
  const parser = new Parser(tokens, filePath);
  return parser.parseProgram();
}

class Parser {
  /**
   * @param {import("./token.mjs").Token[]} tokens
   * @param {string} filePath
   */
  constructor(tokens, filePath) {
    this.tokens = tokens;
    this.filePath = filePath;
    this.index = 0;
  }

  parseProgram() {
    const body = [];
    while (!this.isAtEnd()) {
      body.push(this.parseTopLevelItem());
    }
    return {
      kind: "program",
      body,
    };
  }

  parseTopLevelItem() {
    if (this.matchKeyword("use")) {
      return this.parseUseDecl();
    }
    if (this.matchKeyword("struct")) {
      return this.parseStructDecl();
    }
    if (this.matchKeyword("function")) {
      return this.parseFunctionDecl();
    }
    return this.parseStatement();
  }

  parseUseDecl() {
    const path = [this.expectIdentifier("expected module name").lexeme];
    while (this.matchOperator(".")) {
      path.push(this.expectIdentifier("expected module name").lexeme);
    }
    this.expectStatementTerminator("expected ';' after use declaration");
    return {
      kind: "use_decl",
      path,
    };
  }

  parseStructDecl() {
    const name = this.expectIdentifier("expected struct name").lexeme;
    this.expectOperator("(", "expected '(' after struct name");
    const fields = this.parseParamList(")");
    this.expectOperator(")", "expected ')' after struct fields");
    this.expectStatementTerminator("expected ';' after struct declaration");
    return {
      kind: "struct_decl",
      name,
      fields,
    };
  }

  parseFunctionDecl() {
    let generics = [];
    if (this.matchOperator("<")) {
      generics = this.parseGenericParamList();
      this.expectOperator(">", "expected '>' after generic list");
    }

    const { name, receiverType } = this.parseCallableName();
    this.expectOperator("(", "expected '(' after function name");
    const params = this.parseParamList(")");
    this.expectOperator(")", "expected ')' after parameter list");
    this.expectOperator("->", "expected '->' after parameter list");
    const returnType = this.parseType();
    const body = this.parseBlockStatement();

    return {
      kind: "function_decl",
      name,
      receiverType,
      generics,
      params,
      returnType,
      body,
    };
  }

  parseCallableName() {
    let receiverType = null;
    let name = null;

    if (this.canStartType()) {
      const checkpoint = this.index;
      try {
        const maybeType = this.parseType();
        if (this.matchOperator(".")) {
          receiverType = maybeType;
          name = this.expectIdentifier("expected function name after '.'").lexeme;
        } else {
          this.index = checkpoint;
        }
      } catch {
        this.index = checkpoint;
      }
    }

    if (name == null) {
      name = this.expectIdentifier("expected function name").lexeme;
    }

    return { name, receiverType };
  }

  parseGenericParamList() {
    const params = [];
    if (this.checkOperator(">")) {
      return params;
    }

    do {
      const name = this.expectIdentifier("expected generic parameter name").lexeme;
      let boundType = null;
      if (this.matchOperator(":")) {
        boundType = this.parseType();
      }
      params.push({
        kind: "generic_param",
        name,
        boundType,
      });
    } while (this.matchOperator(","));

    return params;
  }

  parseParamList(endOperator) {
    const params = [];
    if (this.checkOperator(endOperator)) {
      return params;
    }

    do {
      const name = this.expectIdentifier("expected parameter name").lexeme;
      let type = null;
      if (this.matchOperator(":")) {
        type = this.parseType();
      }
      params.push({
        kind: "param",
        name,
        type,
      });
    } while (this.matchOperator(","));

    return params;
  }

  parseBlockStatement() {
    this.expectOperator("{", "expected '{'");
    const statements = [];
    while (!this.isAtEnd() && !this.checkOperator("}")) {
      statements.push(this.parseStatement());
    }
    this.expectOperator("}", "expected '}' after block");
    return {
      kind: "block_stmt",
      statements,
    };
  }

  parseStatement() {
    if (this.checkOperator("{")) {
      return this.parseBlockStatement();
    }

    if (this.matchKeyword("let") || this.matchKeyword("const")) {
      return this.parseBindingStatement(this.previous().lexeme);
    }

    if (this.matchKeyword("if")) {
      return this.parseIfStatement();
    }

    if (this.matchKeyword("while")) {
      return this.parseWhileStatement();
    }

    if (this.matchKeyword("loop")) {
      return this.parseLoopStatement();
    }

    if (this.matchKeyword("for")) {
      return this.parseForStatement();
    }

    if (this.matchKeyword("return")) {
      return this.parseReturnStatement();
    }

    if (this.matchKeyword("break")) {
      this.expectStatementTerminator("expected ';' after break");
      return { kind: "break_stmt" };
    }

    if (this.matchKeyword("continue")) {
      this.expectStatementTerminator("expected ';' after continue");
      return { kind: "continue_stmt" };
    }

    return this.parseExpressionOrAssignmentStatement();
  }

  parseBindingStatement(bindingKind) {
    const name = this.expectIdentifier("expected binding name").lexeme;
    let type = null;
    if (this.matchOperator(":")) {
      type = this.parseType();
    }

    let init = null;
    if (this.matchOperator("=")) {
      init = this.parseExpression();
    }

    this.expectStatementTerminator("expected ';' after binding declaration");

    return {
      kind: "binding_stmt",
      bindingKind,
      name,
      type,
      init,
    };
  }

  parseIfStatement() {
    this.expectOperator("(", "expected '(' after if");
    const condition = this.parseExpression();
    this.expectOperator(")", "expected ')' after if condition");
    const thenBranch = this.parseStatement();
    let elseBranch = null;
    if (this.matchKeyword("else")) {
      elseBranch = this.parseStatement();
    }
    return {
      kind: "if_stmt",
      condition,
      thenBranch,
      elseBranch,
    };
  }

  parseWhileStatement() {
    this.expectOperator("(", "expected '(' after while");
    const condition = this.parseExpression();
    this.expectOperator(")", "expected ')' after while condition");
    const body = this.parseStatement();
    return {
      kind: "while_stmt",
      condition,
      body,
    };
  }

  parseLoopStatement() {
    return {
      kind: "loop_stmt",
      body: this.parseStatement(),
    };
  }

  parseForStatement() {
    this.expectOperator("(", "expected '(' after for");
    const iterator = this.expectIdentifier("expected iterator name").lexeme;
    this.expectOperator(":", "expected ':' after iterator name");
    const iterable = this.parseExpression();
    this.expectOperator(")", "expected ')' after for header");
    const body = this.parseStatement();
    return {
      kind: "for_stmt",
      iterator,
      iterable,
      body,
    };
  }

  parseReturnStatement() {
    if (this.checkOperator(";")) {
      this.advance();
      return {
        kind: "return_stmt",
        value: null,
      };
    }

    if (this.isImplicitTerminator()) {
      return {
        kind: "return_stmt",
        value: null,
      };
    }

    const value = this.parseExpression();
    this.expectStatementTerminator("expected ';' after return");
    return {
      kind: "return_stmt",
      value,
    };
  }

  parseExpressionOrAssignmentStatement() {
    const expression = this.parseExpression();
    if (this.matchAnyOperator(ASSIGNMENT_OPERATORS)) {
      const operator = this.previous().lexeme;
      const value = this.parseExpression();
      this.expectStatementTerminator("expected ';' after assignment");
      return {
        kind: "assign_stmt",
        operator,
        target: expression,
        value,
      };
    }

    this.expectStatementTerminator("expected ';' after expression");
    return {
      kind: "expr_stmt",
      expression,
    };
  }

  parseExpression() {
    return this.parseLogicalOr();
  }

  parseLogicalOr() {
    let expr = this.parseLogicalAnd();
    while (this.matchOperator("||")) {
      expr = {
        kind: "binary_expr",
        operator: "||",
        left: expr,
        right: this.parseLogicalAnd(),
      };
    }
    return expr;
  }

  parseLogicalAnd() {
    let expr = this.parseEquality();
    while (this.matchOperator("&&")) {
      expr = {
        kind: "binary_expr",
        operator: "&&",
        left: expr,
        right: this.parseEquality(),
      };
    }
    return expr;
  }

  parseEquality() {
    let expr = this.parseComparison();
    while (this.matchOperator("==") || this.matchOperator("!=")) {
      const operator = this.previous().lexeme;
      expr = {
        kind: "binary_expr",
        operator,
        left: expr,
        right: this.parseComparison(),
      };
    }
    return expr;
  }

  parseComparison() {
    let expr = this.parseRange();
    while (
      this.matchOperator(">") ||
      this.matchOperator("<") ||
      this.matchOperator(">=") ||
      this.matchOperator("<=")
    ) {
      const operator = this.previous().lexeme;
      expr = {
        kind: "binary_expr",
        operator,
        left: expr,
        right: this.parseRange(),
      };
    }
    return expr;
  }

  parseRange() {
    let expr = this.parseAdditive();
    if (this.matchOperator("..") || this.matchOperator("...")) {
      const operator = this.previous().lexeme;
      const end = this.parseAdditive();
      let step = null;
      if (this.matchOperator("..")) {
        step = this.parseAdditive();
      }
      expr = {
        kind: "range_expr",
        inclusiveEnd: operator === "...",
        start: expr,
        end,
        step,
      };
    }
    return expr;
  }

  parseAdditive() {
    let expr = this.parseMultiplicative();
    while (this.matchOperator("+") || this.matchOperator("-")) {
      const operator = this.previous().lexeme;
      expr = {
        kind: "binary_expr",
        operator,
        left: expr,
        right: this.parseMultiplicative(),
      };
    }
    return expr;
  }

  parseMultiplicative() {
    let expr = this.parseUnary();
    while (this.matchOperator("*") || this.matchOperator("/")) {
      const operator = this.previous().lexeme;
      expr = {
        kind: "binary_expr",
        operator,
        left: expr,
        right: this.parseUnary(),
      };
    }
    return expr;
  }

  parseUnary() {
    if (this.matchAnyOperator(UNARY_OPERATORS)) {
      const operator = this.previous().lexeme;
      return {
        kind: "unary_expr",
        operator,
        operand: this.parseUnary(),
      };
    }
    return this.parsePostfix();
  }

  parsePostfix() {
    let expr = this.parsePrimary();
    while (true) {
      if (this.matchOperator("(")) {
        const args = this.parseArgumentList(")");
        this.expectOperator(")", "expected ')' after argument list");
        expr = {
          kind: "call_expr",
          callee: expr,
          args,
        };
        continue;
      }
      if (this.matchOperator(".")) {
        const member = this.expectIdentifier("expected member name after '.'").lexeme;
        expr = {
          kind: "member_expr",
          object: expr,
          member,
        };
        continue;
      }
      break;
    }
    return expr;
  }

  parseArgumentList(endOperator) {
    const args = [];
    if (this.checkOperator(endOperator)) {
      return args;
    }
    do {
      args.push(this.parseExpression());
    } while (this.matchOperator(","));
    return args;
  }

  parsePrimary() {
    if (this.match(TOKEN_KIND.NUMBER)) {
      return {
        kind: "number_literal",
        raw: this.previous().lexeme,
      };
    }

    if (this.match(TOKEN_KIND.STRING)) {
      return {
        kind: "string_literal",
        raw: this.previous().lexeme,
      };
    }

    if (this.match(TOKEN_KIND.CHAR)) {
      return {
        kind: "char_literal",
        raw: this.previous().lexeme,
      };
    }

    if (this.matchKeyword("true") || this.matchKeyword("false")) {
      return {
        kind: "bool_literal",
        value: this.previous().lexeme === "true",
      };
    }

    if (this.matchKeyword("null")) {
      return {
        kind: "null_literal",
      };
    }

    if (this.matchKeyword("this")) {
      return {
        kind: "this_expr",
      };
    }

    if (this.match(TOKEN_KIND.IDENTIFIER)) {
      return {
        kind: "identifier",
        name: this.previous().lexeme,
      };
    }

    if (this.matchOperator("(")) {
      if (this.matchOperator(")")) {
        return {
          kind: "unit_literal",
        };
      }

      const first = this.parseExpression();
      if (this.matchOperator(",")) {
        const elements = [first];
        do {
          elements.push(this.parseExpression());
        } while (this.matchOperator(","));
        this.expectOperator(")", "expected ')' after tuple");
        return {
          kind: "tuple_literal",
          elements,
        };
      }

      this.expectOperator(")", "expected ')' after expression");
      return {
        kind: "group_expr",
        expression: first,
      };
    }

    if (this.matchOperator("[")) {
      const elements = [];
      if (!this.checkOperator("]")) {
        do {
          elements.push(this.parseExpression());
        } while (this.matchOperator(","));
      }
      this.expectOperator("]", "expected ']' after array literal");
      return {
        kind: "array_literal",
        elements,
      };
    }

    this.throwAtCurrent("expected expression");
  }

  parseType() {
    if (this.matchOperator("[")) {
      const first = this.parseType();
      if (this.matchOperator(":")) {
        const length = this.parseTypeLength();
        this.expectOperator("]", "expected ']' after array type");
        return {
          kind: "array_type",
          elementType: first,
          length,
        };
      }

      if (this.matchOperator(",")) {
        const args = [first];
        do {
          args.push(this.parseType());
        } while (this.matchOperator(","));
        this.expectOperator("]", "expected ']' after receiver type");
        return {
          kind: "generic_receiver_type",
          args,
        };
      }

      this.expectOperator("]", "expected ']' after array type");
      return {
        kind: "array_type",
        elementType: first,
        length: null,
      };
    }

    if (this.matchOperator("(")) {
      if (this.matchOperator(")")) {
        return {
          kind: "unit_type",
        };
      }

      const first = this.parseType();
      if (this.matchOperator(",")) {
        const elements = [first];
        do {
          elements.push(this.parseType());
        } while (this.matchOperator(","));
        this.expectOperator(")", "expected ')' after tuple type");
        return {
          kind: "tuple_type",
          elements,
        };
      }

      this.expectOperator(")", "expected ')' after grouped type");
      return first;
    }

    if (this.match(TOKEN_KIND.IDENTIFIER)) {
      return {
        kind: "named_type",
        name: this.previous().lexeme,
      };
    }

    if (this.check(TOKEN_KIND.KEYWORD) && TYPE_KEYWORDS.has(this.current().lexeme)) {
      const token = this.advance();
      return {
        kind: "named_type",
        name: token.lexeme,
      };
    }

    this.throwAtCurrent("expected type");
  }

  parseTypeLength() {
    if (this.match(TOKEN_KIND.NUMBER) || this.match(TOKEN_KIND.IDENTIFIER)) {
      return this.previous().lexeme;
    }
    this.throwAtCurrent("expected array length");
  }

  canStartType() {
    if (this.check(TOKEN_KIND.IDENTIFIER)) {
      return true;
    }
    if (this.check(TOKEN_KIND.KEYWORD) && TYPE_KEYWORDS.has(this.current().lexeme)) {
      return true;
    }
    return this.check(TOKEN_KIND.OPERATOR) && TYPE_START_OPERATORS.has(this.current().lexeme);
  }

  expectStatementTerminator(message) {
    if (this.matchOperator(";")) {
      return;
    }
    if (this.isImplicitTerminator()) {
      return;
    }
    this.throwAtCurrent(message);
  }

  isImplicitTerminator() {
    if (this.check(TOKEN_KIND.EOF) || this.checkOperator("}")) {
      return true;
    }
    const prev = this.previous();
    const cur = this.current();
    if (prev == null || cur == null) {
      return false;
    }
    return cur.line > prev.endLine;
  }

  isAtEnd() {
    return this.check(TOKEN_KIND.EOF);
  }

  current() {
    return this.tokens[this.index];
  }

  previous() {
    if (this.index === 0) {
      return null;
    }
    return this.tokens[this.index - 1];
  }

  advance() {
    if (!this.isAtEnd()) {
      this.index += 1;
    }
    return this.tokens[this.index - 1];
  }

  /**
   * @param {string} kind
   */
  check(kind) {
    return this.current().kind === kind;
  }

  /**
   * @param {string} operator
   */
  checkOperator(operator) {
    return this.check(TOKEN_KIND.OPERATOR) && this.current().lexeme === operator;
  }

  /**
   * @param {string} keyword
   */
  checkKeyword(keyword) {
    return this.check(TOKEN_KIND.KEYWORD) && this.current().lexeme === keyword;
  }

  /**
   * @param {string} kind
   */
  match(kind) {
    if (!this.check(kind)) {
      return false;
    }
    this.advance();
    return true;
  }

  /**
   * @param {string} operator
   */
  matchOperator(operator) {
    if (!this.checkOperator(operator)) {
      return false;
    }
    this.advance();
    return true;
  }

  /**
   * @param {Set<string>} ops
   */
  matchAnyOperator(ops) {
    if (!this.check(TOKEN_KIND.OPERATOR) || !ops.has(this.current().lexeme)) {
      return false;
    }
    this.advance();
    return true;
  }

  /**
   * @param {string} keyword
   */
  matchKeyword(keyword) {
    if (!this.checkKeyword(keyword)) {
      return false;
    }
    this.advance();
    return true;
  }

  /**
   * @param {string} operator
   * @param {string} message
   */
  expectOperator(operator, message) {
    if (!this.matchOperator(operator)) {
      this.throwAtCurrent(message);
    }
  }

  /**
   * @param {string} message
   */
  expectIdentifier(message) {
    if (!this.match(TOKEN_KIND.IDENTIFIER)) {
      this.throwAtCurrent(message);
    }
    return this.previous();
  }

  /**
   * @param {string} message
   */
  throwAtCurrent(message) {
    const token = this.current();
    throw new MdrCompileError(message, {
      filePath: this.filePath,
      line: token.line,
      column: token.column,
    });
  }
}
