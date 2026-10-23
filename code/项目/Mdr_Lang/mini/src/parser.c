#include "mini_internal.h"

#include <string.h>

static bool mini_token_equals(const mini_token *tok, const char *text) {
    return tok->kind == MINI_TOKEN_OPERATOR && tok->len == strlen(text) &&
           strncmp(tok->begin, text, tok->len) == 0;
}

static bool mini_token_keyword(const mini_token *tok, const char *text) {
    return tok->kind == MINI_TOKEN_KEYWORD && tok->len == strlen(text) &&
           strncmp(tok->begin, text, tok->len) == 0;
}

static const mini_token *parser_current(const mini_parser *p) {
    return &p->tokens[p->index];
}

static bool parser_at_end(const mini_parser *p) {
    return parser_current(p)->kind == MINI_TOKEN_EOF;
}

static void parser_advance(mini_parser *p) {
    if (!parser_at_end(p)) {
        p->index += 1u;
    }
}

static const mini_token *parser_previous(const mini_parser *p) {
    return &p->tokens[p->index > 0u ? p->index - 1u : 0u];
}

static bool parser_fail(mini_parser *p, const mini_token *where, const char *message) {
    mini_set_error(p->error, false, where->line, where->column, "%s", message);
    return false;
}

static bool parser_expect_operator(mini_parser *p, const char *op, const char *message) {
    if (!mini_token_equals(parser_current(p), op)) {
        return parser_fail(p, parser_current(p), message);
    }
    parser_advance(p);
    return true;
}

static bool parser_match_operator(mini_parser *p, const char *op) {
    if (!mini_token_equals(parser_current(p), op)) {
        return false;
    }
    parser_advance(p);
    return true;
}

static bool parser_match_keyword(mini_parser *p, const char *word) {
    if (!mini_token_keyword(parser_current(p), word)) {
        return false;
    }
    parser_advance(p);
    return true;
}

static bool parser_is_implicit_terminator(const mini_parser *p) {
    const mini_token *cur = parser_current(p);
    const mini_token *prev = parser_previous(p);
    if (cur->kind == MINI_TOKEN_EOF) {
        return true;
    }
    if (mini_token_equals(cur, "}")) {
        return true;
    }
    return cur->line > prev->end_line;
}

static bool parser_terminator(mini_parser *p, const char *message) {
    if (parser_match_operator(p, ";")) {
        return true;
    }
    if (parser_is_implicit_terminator(p)) {
        return true;
    }
    return parser_fail(p, parser_current(p), message);
}

static node *parser_expression(mini_parser *p);
static node *parser_statement(mini_parser *p);

static node *parser_primary(mini_parser *p) {
    const mini_token *tok = parser_current(p);

    if (tok->kind == MINI_TOKEN_INT) {
        node *n = node_new(p->a, NODE_NUMBER_LITERAL);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.int_value = tok->int_value;
        parser_advance(p);
        return n;
    }

    if (mini_token_keyword(tok, "true") || mini_token_keyword(tok, "false")) {
        node *n = node_new(p->a, NODE_BOOL_LITERAL);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.bool_value = mini_token_keyword(tok, "true");
        parser_advance(p);
        return n;
    }

    if (tok->kind == MINI_TOKEN_IDENTIFIER) {
        node *n = node_new(p->a, NODE_IDENTIFIER);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.identifier.name = tok->begin;
        n->as.identifier.len = tok->len;
        parser_advance(p);
        return n;
    }

    if (parser_match_operator(p, "(")) {
        node *inner = parser_expression(p);
        if (inner == NULL) {
            return NULL;
        }
        if (!parser_expect_operator(p, ")", "expected ')' after expression")) {
            return NULL;
        }
        node *n = node_new(p->a, NODE_GROUP_EXPR);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.group.inner = inner;
        return n;
    }

    parser_fail(p, tok, "expected expression");
    return NULL;
}

static node *parser_unary(mini_parser *p) {
    const mini_token *tok = parser_current(p);
    operator_kind op;
    bool is_unary = false;
    if (mini_token_equals(tok, "-")) {
        op = OP_NEG;
        is_unary = true;
    } else if (mini_token_equals(tok, "!")) {
        op = OP_NOT;
        is_unary = true;
    }
    if (is_unary) {
        parser_advance(p);
        node *operand = parser_unary(p);
        if (operand == NULL) {
            return NULL;
        }
        node *n = node_new(p->a, NODE_UNARY_EXPR);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.unary.op = op;
        n->as.unary.operand = operand;
        return n;
    }
    return parser_primary(p);
}

static node *parser_binary_left(mini_parser *p, node *(*next)(mini_parser *),
                                const char *const *ops, const operator_kind *kinds,
                                size_t op_count) {
    node *left = next(p);
    if (left == NULL) {
        return NULL;
    }
    for (;;) {
        ptrdiff_t matched = -1;
        for (size_t i = 0; i < op_count; ++i) {
            if (mini_token_equals(parser_current(p), ops[i])) {
                matched = (ptrdiff_t)i;
                break;
            }
        }
        if (matched < 0) {
            break;
        }
        const mini_token *op_tok = parser_current(p);
        parser_advance(p);
        node *right = next(p);
        if (right == NULL) {
            return NULL;
        }
        node *n = node_new(p->a, NODE_BINARY_EXPR);
        if (n == NULL) {
            mini_set_error(p->error, false, op_tok->line, op_tok->column, "out of memory");
            return NULL;
        }
        n->line = op_tok->line;
        n->column = op_tok->column;
        n->as.binary.op = kinds[matched];
        n->as.binary.left = left;
        n->as.binary.right = right;
        left = n;
    }
    return left;
}

static node *parser_multiplicative(mini_parser *p) {
    static const char *const ops[] = {"*", "/"};
    static const operator_kind kinds[] = {OP_MUL, OP_DIV};
    return parser_binary_left(p, parser_unary, ops, kinds, 2u);
}

static node *parser_additive(mini_parser *p) {
    static const char *const ops[] = {"+", "-"};
    static const operator_kind kinds[] = {OP_ADD, OP_SUB};
    return parser_binary_left(p, parser_multiplicative, ops, kinds, 2u);
}

static node *parser_range(mini_parser *p) {
    node *left = parser_additive(p);
    if (left == NULL) {
        return NULL;
    }
    bool inclusive = false;
    const mini_token *op_tok = parser_current(p);
    if (mini_token_equals(op_tok, "..")) {
        inclusive = false;
    } else if (mini_token_equals(op_tok, "...")) {
        inclusive = true;
    } else {
        return left;
    }
    parser_advance(p);
    node *end = parser_additive(p);
    if (end == NULL) {
        return NULL;
    }
    node *step = NULL;
    if (mini_token_equals(parser_current(p), "..")) {
        parser_advance(p);
        step = parser_additive(p);
        if (step == NULL) {
            return NULL;
        }
    }
    node *n = node_new(p->a, NODE_RANGE_EXPR);
    if (n == NULL) {
        mini_set_error(p->error, false, op_tok->line, op_tok->column, "out of memory");
        return NULL;
    }
    n->line = op_tok->line;
    n->column = op_tok->column;
    n->as.range.start = left;
    n->as.range.end = end;
    n->as.range.step = step;
    n->as.range.inclusive_end = inclusive;
    return n;
}

static node *parser_comparison(mini_parser *p) {
    static const char *const ops[] = {"<", "<=", ">", ">="};
    static const operator_kind kinds[] = {OP_LT, OP_LE, OP_GT, OP_GE};
    return parser_binary_left(p, parser_range, ops, kinds, 4u);
}

static node *parser_equality(mini_parser *p) {
    static const char *const ops[] = {"==", "!="};
    static const operator_kind kinds[] = {OP_EQ, OP_NE};
    return parser_binary_left(p, parser_comparison, ops, kinds, 2u);
}

static node *parser_logical_and(mini_parser *p) {
    static const char *const ops[] = {"&&"};
    static const operator_kind kinds[] = {OP_AND};
    return parser_binary_left(p, parser_equality, ops, kinds, 1u);
}

static node *parser_logical_or(mini_parser *p) {
    static const char *const ops[] = {"||"};
    static const operator_kind kinds[] = {OP_OR};
    return parser_binary_left(p, parser_logical_and, ops, kinds, 1u);
}

static node *parser_expression(mini_parser *p) {
    return parser_logical_or(p);
}

static node *parser_block(mini_parser *p) {
    const mini_token *open = parser_current(p);
    if (!parser_expect_operator(p, "{", "expected '{'")) {
        return NULL;
    }
    node *block = node_new(p->a, NODE_BLOCK_STMT);
    if (block == NULL) {
        mini_set_error(p->error, false, open->line, open->column, "out of memory");
        return NULL;
    }
    block->line = open->line;
    block->column = open->column;

    while (!parser_at_end(p) && !mini_token_equals(parser_current(p), "}")) {
        node *stmt = parser_statement(p);
        if (stmt == NULL) {
            return NULL;
        }
        if (!node_list_push(p->a, &block->as.block.statements, stmt)) {
            mini_set_error(p->error, false, stmt->line, stmt->column, "out of memory");
            return NULL;
        }
    }
    if (!parser_expect_operator(p, "}", "expected '}' after block")) {
        return NULL;
    }
    return block;
}

static node *parser_statement(mini_parser *p) {
    const mini_token *tok = parser_current(p);

    if (mini_token_equals(tok, "{")) {
        return parser_block(p);
    }

    if (mini_token_keyword(tok, "let") || mini_token_keyword(tok, "const")) {
        bool is_const = mini_token_keyword(tok, "const");
        parser_advance(p);
        const mini_token *name = parser_current(p);
        if (name->kind != MINI_TOKEN_IDENTIFIER) {
            parser_fail(p, name, "expected binding name");
            return NULL;
        }
        parser_advance(p);
        if (!parser_expect_operator(p, "=", "expected '=' after binding name")) {
            return NULL;
        }
        node *init = parser_expression(p);
        if (init == NULL) {
            return NULL;
        }
        if (!parser_terminator(p, "expected ';' after binding declaration")) {
            return NULL;
        }
        node *n = node_new(p->a, NODE_BINDING_STMT);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.binding.name = name->begin;
        n->as.binding.len = name->len;
        n->as.binding.initializer = init;
        n->as.binding.is_const = is_const;
        return n;
    }

    if (mini_token_keyword(tok, "if")) {
        parser_advance(p);
        if (!parser_expect_operator(p, "(", "expected '(' after if")) {
            return NULL;
        }
        node *cond = parser_expression(p);
        if (cond == NULL) {
            return NULL;
        }
        if (!parser_expect_operator(p, ")", "expected ')' after if condition")) {
            return NULL;
        }
        node *then_branch = parser_statement(p);
        if (then_branch == NULL) {
            return NULL;
        }
        node *else_branch = NULL;
        if (parser_match_keyword(p, "else")) {
            else_branch = parser_statement(p);
            if (else_branch == NULL) {
                return NULL;
            }
        }
        node *n = node_new(p->a, NODE_IF_STMT);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.if_stmt.condition = cond;
        n->as.if_stmt.then_branch = then_branch;
        n->as.if_stmt.else_branch = else_branch;
        return n;
    }

    if (mini_token_keyword(tok, "while")) {
        parser_advance(p);
        if (!parser_expect_operator(p, "(", "expected '(' after while")) {
            return NULL;
        }
        node *cond = parser_expression(p);
        if (cond == NULL) {
            return NULL;
        }
        if (!parser_expect_operator(p, ")", "expected ')' after while condition")) {
            return NULL;
        }
        p->loop_depth += 1u;
        node *body = parser_statement(p);
        p->loop_depth -= 1u;
        if (body == NULL) {
            return NULL;
        }
        node *n = node_new(p->a, NODE_WHILE_STMT);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.while_stmt.condition = cond;
        n->as.while_stmt.body = body;
        return n;
    }

    if (mini_token_keyword(tok, "loop")) {
        parser_advance(p);
        p->loop_depth += 1u;
        node *body = parser_statement(p);
        p->loop_depth -= 1u;
        if (body == NULL) {
            return NULL;
        }
        node *n = node_new(p->a, NODE_LOOP_STMT);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.loop_stmt.body = body;
        return n;
    }

    if (mini_token_keyword(tok, "for")) {
        parser_advance(p);
        if (!parser_expect_operator(p, "(", "expected '(' after for")) {
            return NULL;
        }
        const mini_token *name = parser_current(p);
        if (name->kind != MINI_TOKEN_IDENTIFIER) {
            parser_fail(p, name, "expected iterator name");
            return NULL;
        }
        parser_advance(p);
        if (!parser_expect_operator(p, ":", "expected ':' after iterator name")) {
            return NULL;
        }
        node *iterable = parser_expression(p);
        if (iterable == NULL) {
            return NULL;
        }
        if (!parser_expect_operator(p, ")", "expected ')' after for header")) {
            return NULL;
        }
        p->loop_depth += 1u;
        node *body = parser_statement(p);
        p->loop_depth -= 1u;
        if (body == NULL) {
            return NULL;
        }
        node *n = node_new(p->a, NODE_FOR_STMT);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.for_stmt.name = name->begin;
        n->as.for_stmt.len = name->len;
        n->as.for_stmt.iterable = iterable;
        n->as.for_stmt.body = body;
        return n;
    }

    if (mini_token_keyword(tok, "break") || mini_token_keyword(tok, "continue")) {
        bool is_break = mini_token_keyword(tok, "break");
        if (p->loop_depth == 0u) {
            parser_fail(p, tok, is_break ? "break is only allowed in loops"
                                         : "continue is only allowed in loops");
            return NULL;
        }
        parser_advance(p);
        if (!parser_terminator(p, is_break ? "expected ';' after break" : "expected ';' after continue")) {
            return NULL;
        }
        node *n = node_new(p->a, is_break ? NODE_BREAK_STMT : NODE_CONTINUE_STMT);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        return n;
    }

    if (mini_token_keyword(tok, "print")) {
        parser_advance(p);
        if (!parser_expect_operator(p, "(", "expected '(' after print")) {
            return NULL;
        }
        node *value = parser_expression(p);
        if (value == NULL) {
            return NULL;
        }
        if (!parser_expect_operator(p, ")", "expected ')' after print argument")) {
            return NULL;
        }
        if (!parser_terminator(p, "expected ';' after print")) {
            return NULL;
        }
        node *n = node_new(p->a, NODE_PRINT_STMT);
        if (n == NULL) {
            mini_set_error(p->error, false, tok->line, tok->column, "out of memory");
            return NULL;
        }
        n->line = tok->line;
        n->column = tok->column;
        n->as.print_stmt.value = value;
        return n;
    }

    node *expr = parser_expression(p);
    if (expr == NULL) {
        return NULL;
    }

    static const char *const assign_ops[] = {"=", "+=", "-=", "*=", "/="};
    static const operator_kind assign_kinds[] = {OP_ASSIGN, OP_ADD, OP_SUB, OP_MUL, OP_DIV};
    ptrdiff_t matched = -1;
    for (size_t i = 0; i < 5u; ++i) {
        if (mini_token_equals(parser_current(p), assign_ops[i])) {
            matched = (ptrdiff_t)i;
            break;
        }
    }
    if (matched >= 0) {
        const mini_token *op_tok = parser_current(p);
        parser_advance(p);
        if (expr->kind != NODE_IDENTIFIER) {
            parser_fail(p, op_tok, "left side of assignment must be a name");
            return NULL;
        }
        node *value = parser_expression(p);
        if (value == NULL) {
            return NULL;
        }
        if (!parser_terminator(p, "expected ';' after assignment")) {
            return NULL;
        }
        node *n = node_new(p->a, NODE_ASSIGN_STMT);
        if (n == NULL) {
            mini_set_error(p->error, false, op_tok->line, op_tok->column, "out of memory");
            return NULL;
        }
        n->line = op_tok->line;
        n->column = op_tok->column;
        n->as.assign.name = expr->as.identifier.name;
        n->as.assign.len = expr->as.identifier.len;
        n->as.assign.value = value;
        n->as.assign.op = assign_kinds[matched];
        return n;
    }

    if (!parser_terminator(p, "expected ';' after expression")) {
        return NULL;
    }
    return expr;
}

bool mini_parse(const char *source, arena *a, node **out_program, mdr_mini_error *error) {
    mini_token *tokens = NULL;
    size_t count = 0u;
    if (!mini_lex(source, &tokens, &count, error)) {
        return false;
    }

    mini_parser p = {.tokens = tokens, .count = count, .index = 0u, .a = a, .error = error};
    node *program = node_new(a, NODE_BLOCK_STMT);
    if (program == NULL) {
        mini_lex_free(tokens);
        mini_set_error(error, false, 1u, 1u, "out of memory");
        return false;
    }
    program->line = 1u;
    program->column = 1u;

    while (!parser_at_end(&p)) {
        node *stmt = parser_statement(&p);
        if (stmt == NULL) {
            mini_lex_free(tokens);
            return false;
        }
        if (!node_list_push(a, &program->as.block.statements, stmt)) {
            mini_lex_free(tokens);
            mini_set_error(error, false, stmt->line, stmt->column, "out of memory");
            return false;
        }
    }

    mini_lex_free(tokens);
    *out_program = program;
    return true;
}