#include "../include/mdr_compiler.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_KEYWORD,
    TOKEN_OPERATOR
} token_kind;

typedef struct {
    token_kind kind;
    const char *begin;
    size_t len;
    size_t line;
    size_t column;
    size_t end_line;
    size_t end_column;
} token;

typedef struct {
    token *items;
    size_t len;
    size_t cap;
} token_vec;

typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_UNIT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_NUMBER_LITERAL,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_F32,
    TYPE_F64,
    TYPE_NAMED,
    TYPE_ARRAY,
    TYPE_TUPLE,
    TYPE_RANGE,
    TYPE_NULL
} semantic_type_kind;

typedef struct {
    semantic_type_kind kind;
    const char *name;
    size_t name_len;
} semantic_type;

typedef struct {
    const char *file_path;
    const char *source;
    size_t length;
    size_t offset;
    size_t line;
    size_t column;
} lexer;

typedef struct {
    const char *file_path;
    const token *tokens;
    size_t count;
    size_t index;
    mdr_error *error;
    struct semantic_symbol *symbols;
    size_t symbol_count;
    size_t symbol_cap;
    size_t *scope_marks;
    size_t scope_count;
    size_t scope_cap;
    size_t loop_depth;
    size_t function_depth;
    semantic_type current_function_return_type;
} parser;

typedef enum {
    SYMBOL_OBJECT = 0,
    SYMBOL_FUNCTION,
    SYMBOL_STRUCT,
    SYMBOL_TYPE_PARAM
} symbol_kind;

typedef struct semantic_symbol {
    char *name;
    size_t len;
    symbol_kind kind;
    size_t scope_depth;
    semantic_type type;
    semantic_type *param_types;
    size_t param_count;
    char **member_names;
    size_t *member_name_lens;
    size_t member_count;
    bool has_receiver;
    semantic_type receiver_type;
    size_t arity;
    bool has_arity;
    bool variadic;
} semantic_symbol;

typedef struct {
    char *name;
    size_t len;
    size_t line;
    size_t column;
    bool is_value_param;
    semantic_type type;
} name_entry;

typedef struct {
    name_entry *items;
    size_t len;
    size_t cap;
} name_vec;

typedef enum {
    CALLEE_NONE = 0,
    CALLEE_IDENTIFIER,
    CALLEE_MEMBER
} callee_kind;

typedef struct {
    callee_kind kind;
    const token *name_tok;
    const semantic_symbol *symbol;
} expr_meta;

typedef struct {
    semantic_type type;
    bool assignable;
    expr_meta callee;
} expr_result;

typedef struct {
    semantic_type *items;
    size_t len;
    size_t cap;
} type_vec;

static const char *const k_keywords[] = {
    "use",      "function", "let",     "const", "struct", "if",   "else",
    "while",    "loop",     "return",  "break", "continue", "for",
    "true",     "false",    "this",    "null", "i8", "i16", "i32", "i64",
    "u8",       "u16",      "u32",     "u64", "f32", "f64", "bool", "char",
};

static const char *const k_type_keywords[] = {
    "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "bool", "char",
};

static const char *const k_operators[] = {
    "...", "..", "->", "||", "&&", "==", "!=", ">=", "<=", "+=", "-=", "*=", "/=",
    "=",   ">",  "<",  "+",  "-",  "*",  "/",  "!",  ".",  ",",  ":",  ";",
    "(",   ")",  "{",  "}",  "[",  "]",
};

static bool token_vec_push(token_vec *vec, token item);
static void token_vec_free(token_vec *vec);

static bool lex_source(const char *file_path, const char *source, token_vec *out_tokens, mdr_error *error);
static bool parse_program(parser *p);

static bool parse_top_level_item(parser *p);
static bool parse_use_decl(parser *p);
static bool parse_struct_decl(parser *p);
static bool parse_function_decl(parser *p);
static bool parse_generic_param_list(parser *p, name_vec *names);
static bool parse_param_list(parser *p, const char *end_op, name_vec *names);
static bool parse_block_statement(parser *p);
static bool parse_function_block(parser *p,
                                 const name_vec *generic_names,
                                 const name_vec *param_names,
                                 bool has_receiver,
                                 semantic_type receiver_type);
static bool parse_statement(parser *p);
static bool parse_binding_statement(parser *p);
static bool parse_if_statement(parser *p);
static bool parse_while_statement(parser *p);
static bool parse_loop_statement(parser *p);
static bool parse_for_statement(parser *p);
static bool parse_return_statement(parser *p);
static bool parse_expression_or_assignment_statement(parser *p);

static bool parse_expression(parser *p, expr_result *out);
static bool parse_logical_or(parser *p, expr_result *out);
static bool parse_logical_and(parser *p, expr_result *out);
static bool parse_equality(parser *p, expr_result *out);
static bool parse_comparison(parser *p, expr_result *out);
static bool parse_range(parser *p, expr_result *out);
static bool parse_additive(parser *p, expr_result *out);
static bool parse_multiplicative(parser *p, expr_result *out);
static bool parse_unary(parser *p, expr_result *out);
static bool parse_postfix(parser *p, expr_result *out);
static bool parse_primary(parser *p, expr_result *out);
static bool parse_argument_list(parser *p, const char *end_op, type_vec *arg_types);

static bool parse_type(parser *p, semantic_type *out);
static bool parse_type_length(parser *p);

static bool parse_statement_terminator(parser *p, const char *message);
static bool is_implicit_terminator(const parser *p);

static bool parser_match_kind(parser *p, token_kind kind);
static bool parser_match_keyword(parser *p, const char *keyword);
static bool parser_match_operator(parser *p, const char *op);
static bool parser_expect_identifier(parser *p, const char *message);
static bool parser_expect_operator(parser *p, const char *op, const char *message);
static bool parser_check_kind(const parser *p, token_kind kind);
static bool parser_check_keyword(const parser *p, const char *keyword);
static bool parser_check_operator(const parser *p, const char *op);
static bool parser_check_operator_n(const parser *p, size_t n, const char *op);
static const token *parser_current(const parser *p);
static const token *parser_previous(const parser *p);
static const token *parser_peek_n(const parser *p, size_t n);
static bool parser_is_at_end(const parser *p);
static void parser_advance(parser *p);
static bool parser_fail(parser *p, const token *where, const char *message);

static bool token_text_eq(const token *tok, const char *text);
static bool token_is_type_keyword(const token *tok);
static bool token_is_assignment_operator(const token *tok);
static bool token_is_unary_operator(const token *tok);

static bool parser_begin_scope(parser *p);
static void parser_end_scope(parser *p);
static bool parser_declare_token(parser *p, const token *tok, symbol_kind kind, const char *duplicate_message);
static bool parser_declare_name(parser *p,
                                const char *name,
                                size_t len,
                                symbol_kind kind,
                                size_t line,
                                size_t column,
                                const char *duplicate_message);
static const semantic_symbol *parser_lookup(parser *p, const token *tok);
static semantic_symbol *parser_lookup_mut(parser *p, const token *tok);
static const semantic_symbol *parser_lookup_name_kind(parser *p,
                                                      const char *name,
                                                      size_t len,
                                                      symbol_kind kind);
static bool parser_name_equals(const semantic_symbol *sym, const char *name, size_t len);
static bool parser_require_identifier(parser *p, const token *tok);
static bool parser_is_builtin_name(const token *tok);
static void parser_cleanup(parser *p);

static bool name_vec_push(name_vec *vec, const token *tok, bool is_value_param, semantic_type type);
static void name_vec_free(name_vec *vec);
static bool type_vec_push(type_vec *vec, semantic_type type);
static void type_vec_free(type_vec *vec);

static semantic_type semantic_type_unknown(void);
static semantic_type semantic_type_unit(void);
static semantic_type semantic_type_named(const char *name, size_t len);
static semantic_type semantic_type_of_symbol(const semantic_symbol *sym);
static bool semantic_type_is_unknown(semantic_type type);
static bool semantic_type_is_unit(semantic_type type);
static bool semantic_type_is_bool(semantic_type type);
static bool semantic_type_is_numeric(semantic_type type);
static bool semantic_type_is_array_or_range(semantic_type type);
static bool semantic_type_equal(semantic_type lhs, semantic_type rhs);
static bool semantic_type_compatible(semantic_type expected, semantic_type actual);
static semantic_type semantic_type_from_type_keyword(const token *tok);
static semantic_type semantic_common_numeric_type(semantic_type lhs, semantic_type rhs);
static void semantic_type_format(semantic_type type, char *buf, size_t buf_size);
static bool semantic_build_method_key(semantic_type receiver_type,
                                      const char *member_name,
                                      size_t member_len,
                                      char **out_key,
                                      size_t *out_len);
static const semantic_symbol *semantic_lookup_method_symbol(parser *p,
                                                            semantic_type receiver_type,
                                                            const token *member_tok);
static bool semantic_lookup_struct_field(const semantic_symbol *struct_symbol,
                                         const token *member_tok,
                                         semantic_type *out_type);
static bool parser_fail_type_mismatch(parser *p,
                                      const token *where,
                                      const char *context,
                                      semantic_type expected,
                                      semantic_type actual);

static void set_error(mdr_error *error,
                      const char *file_path,
                      size_t line,
                      size_t column,
                      const char *message);

static const char *token_kind_name(token_kind kind);

bool mdr_compile_text(const char *file_path, const char *source, bool dump_tokens, mdr_error *error) {
    token_vec tokens = {0};
    if (!lex_source(file_path, source, &tokens, error)) {
        token_vec_free(&tokens);
        return false;
    }

    if (dump_tokens) {
        for (size_t i = 0; i < tokens.len; ++i) {
            const token *tok = &tokens.items[i];
            printf("%5zu:%-4zu %-10s %.*s\n",
                   tok->line,
                   tok->column,
                   token_kind_name(tok->kind),
                   (int)tok->len,
                   tok->begin);
        }
    }

    parser p = {
        .file_path = file_path,
        .tokens = tokens.items,
        .count = tokens.len,
        .index = 0u,
        .error = error,
        .symbols = NULL,
        .symbol_count = 0u,
        .symbol_cap = 0u,
        .scope_marks = NULL,
        .scope_count = 0u,
        .scope_cap = 0u,
        .loop_depth = 0u,
        .function_depth = 0u,
        .current_function_return_type = semantic_type_unknown(),
    };

    bool ok = parse_program(&p);
    parser_cleanup(&p);
    token_vec_free(&tokens);
    return ok;
}

static bool lex_source(const char *file_path, const char *source, token_vec *out_tokens, mdr_error *error) {
    lexer lx = {
        .file_path = file_path,
        .source = source,
        .length = strlen(source),
        .offset = 0u,
        .line = 1u,
        .column = 1u,
    };

    while (lx.offset < lx.length) {
        char c = lx.source[lx.offset];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') {
                lx.line += 1u;
                lx.column = 1u;
            } else {
                lx.column += 1u;
            }
            lx.offset += 1u;
            continue;
        }

        if (c == '/' && lx.offset + 1u < lx.length && lx.source[lx.offset + 1u] == '/') {
            lx.offset += 2u;
            lx.column += 2u;
            while (lx.offset < lx.length && lx.source[lx.offset] != '\n') {
                lx.offset += 1u;
                lx.column += 1u;
            }
            continue;
        }

        size_t start = lx.offset;
        size_t start_line = lx.line;
        size_t start_col = lx.column;

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            lx.offset += 1u;
            lx.column += 1u;
            while (lx.offset < lx.length) {
                char ch = lx.source[lx.offset];
                bool is_name = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                               (ch >= '0' && ch <= '9') || ch == '_';
                if (!is_name) {
                    break;
                }
                lx.offset += 1u;
                lx.column += 1u;
            }

            token t = {
                .kind = TOKEN_IDENTIFIER,
                .begin = lx.source + start,
                .len = lx.offset - start,
                .line = start_line,
                .column = start_col,
                .end_line = lx.line,
                .end_column = lx.column,
            };

            for (size_t i = 0; i < sizeof(k_keywords) / sizeof(k_keywords[0]); ++i) {
                if (strlen(k_keywords[i]) == t.len && strncmp(t.begin, k_keywords[i], t.len) == 0) {
                    t.kind = TOKEN_KEYWORD;
                    break;
                }
            }

            if (!token_vec_push(out_tokens, t)) {
                set_error(error, file_path, start_line, start_col, "out of memory");
                return false;
            }
            continue;
        }

        if (c >= '0' && c <= '9') {
            lx.offset += 1u;
            lx.column += 1u;
            while (lx.offset < lx.length && isdigit((unsigned char)lx.source[lx.offset])) {
                lx.offset += 1u;
                lx.column += 1u;
            }

            if (lx.offset + 1u < lx.length && lx.source[lx.offset] == '.' &&
                lx.source[lx.offset + 1u] != '.' && isdigit((unsigned char)lx.source[lx.offset + 1u])) {
                lx.offset += 1u;
                lx.column += 1u;
                while (lx.offset < lx.length && isdigit((unsigned char)lx.source[lx.offset])) {
                    lx.offset += 1u;
                    lx.column += 1u;
                }
            }

            if (lx.offset < lx.length && (lx.source[lx.offset] == 'e' || lx.source[lx.offset] == 'E')) {
                size_t save_off = lx.offset;
                size_t save_col = lx.column;
                lx.offset += 1u;
                lx.column += 1u;
                if (lx.offset < lx.length && (lx.source[lx.offset] == '+' || lx.source[lx.offset] == '-')) {
                    lx.offset += 1u;
                    lx.column += 1u;
                }
                if (lx.offset >= lx.length || !isdigit((unsigned char)lx.source[lx.offset])) {
                    lx.offset = save_off;
                    lx.column = save_col;
                } else {
                    while (lx.offset < lx.length && isdigit((unsigned char)lx.source[lx.offset])) {
                        lx.offset += 1u;
                        lx.column += 1u;
                    }
                }
            }

            while (lx.offset < lx.length) {
                char ch = lx.source[lx.offset];
                bool is_suffix = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                 (ch >= '0' && ch <= '9') || ch == '_';
                if (!is_suffix) {
                    break;
                }
                lx.offset += 1u;
                lx.column += 1u;
            }

            token t = {
                .kind = TOKEN_NUMBER,
                .begin = lx.source + start,
                .len = lx.offset - start,
                .line = start_line,
                .column = start_col,
                .end_line = lx.line,
                .end_column = lx.column,
            };
            if (!token_vec_push(out_tokens, t)) {
                set_error(error, file_path, start_line, start_col, "out of memory");
                return false;
            }
            continue;
        }

        if (c == '"') {
            lx.offset += 1u;
            lx.column += 1u;
            bool done = false;
            while (lx.offset < lx.length) {
                char ch = lx.source[lx.offset];
                if (ch == '"') {
                    lx.offset += 1u;
                    lx.column += 1u;
                    done = true;
                    break;
                }
                if (ch == '\\') {
                    lx.offset += 1u;
                    lx.column += 1u;
                    if (lx.offset >= lx.length) {
                        break;
                    }
                    if (lx.source[lx.offset] == '\n') {
                        set_error(error, file_path, start_line, start_col, "unterminated string literal");
                        return false;
                    }
                    lx.offset += 1u;
                    lx.column += 1u;
                    continue;
                }
                if (ch == '\n') {
                    set_error(error, file_path, start_line, start_col, "unterminated string literal");
                    return false;
                }
                lx.offset += 1u;
                lx.column += 1u;
            }
            if (!done) {
                set_error(error, file_path, start_line, start_col, "unterminated string literal");
                return false;
            }

            token t = {
                .kind = TOKEN_STRING,
                .begin = lx.source + start,
                .len = lx.offset - start,
                .line = start_line,
                .column = start_col,
                .end_line = lx.line,
                .end_column = lx.column,
            };
            if (!token_vec_push(out_tokens, t)) {
                set_error(error, file_path, start_line, start_col, "out of memory");
                return false;
            }
            continue;
        }

        if (c == '\'') {
            lx.offset += 1u;
            lx.column += 1u;
            if (lx.offset >= lx.length || lx.source[lx.offset] == '\n') {
                set_error(error, file_path, start_line, start_col, "unterminated char literal");
                return false;
            }
            if (lx.source[lx.offset] == '\\') {
                lx.offset += 1u;
                lx.column += 1u;
                if (lx.offset >= lx.length) {
                    set_error(error, file_path, start_line, start_col, "unterminated char literal");
                    return false;
                }
            }
            lx.offset += 1u;
            lx.column += 1u;
            if (lx.offset >= lx.length || lx.source[lx.offset] != '\'') {
                set_error(error, file_path, start_line, start_col, "invalid char literal");
                return false;
            }
            lx.offset += 1u;
            lx.column += 1u;

            token t = {
                .kind = TOKEN_CHAR,
                .begin = lx.source + start,
                .len = lx.offset - start,
                .line = start_line,
                .column = start_col,
                .end_line = lx.line,
                .end_column = lx.column,
            };
            if (!token_vec_push(out_tokens, t)) {
                set_error(error, file_path, start_line, start_col, "out of memory");
                return false;
            }
            continue;
        }

        bool matched = false;
        for (size_t i = 0; i < sizeof(k_operators) / sizeof(k_operators[0]); ++i) {
            size_t op_len = strlen(k_operators[i]);
            if (lx.offset + op_len > lx.length) {
                continue;
            }
            if (strncmp(lx.source + lx.offset, k_operators[i], op_len) == 0) {
                lx.offset += op_len;
                lx.column += op_len;
                token t = {
                    .kind = TOKEN_OPERATOR,
                    .begin = lx.source + start,
                    .len = op_len,
                    .line = start_line,
                    .column = start_col,
                    .end_line = lx.line,
                    .end_column = lx.column,
                };
                if (!token_vec_push(out_tokens, t)) {
                    set_error(error, file_path, start_line, start_col, "out of memory");
                    return false;
                }
                matched = true;
                break;
            }
        }
        if (matched) {
            continue;
        }

        set_error(error, file_path, start_line, start_col, "unexpected character");
        return false;
    }

    token eof = {
        .kind = TOKEN_EOF,
        .begin = lx.source + lx.offset,
        .len = 0u,
        .line = lx.line,
        .column = lx.column,
        .end_line = lx.line,
        .end_column = lx.column,
    };
    if (!token_vec_push(out_tokens, eof)) {
        set_error(error, file_path, lx.line, lx.column, "out of memory");
        return false;
    }
    return true;
}

static bool parse_program(parser *p) {
    if (!parser_begin_scope(p)) {
        return parser_fail(p, parser_current(p), "out of memory");
    }

    if (!parser_declare_name(p, "print", 5u, SYMBOL_FUNCTION, 1u, 1u, "duplicate declaration")) {
        return false;
    }
    token print_tok = {
        .kind = TOKEN_IDENTIFIER,
        .begin = "print",
        .len = 5u,
        .line = 1u,
        .column = 1u,
        .end_line = 1u,
        .end_column = 6u,
    };
    semantic_symbol *print_sym = parser_lookup_mut(p, &print_tok);
    if (print_sym != NULL) {
        print_sym->type = semantic_type_unit();
        print_sym->has_arity = true;
        print_sym->arity = 1u;
        print_sym->variadic = true;
    }

    while (!parser_is_at_end(p)) {
        if (!parse_top_level_item(p)) {
            return false;
        }
    }
    return true;
}

static bool parse_top_level_item(parser *p) {
    if (parser_match_keyword(p, "use")) {
        return parse_use_decl(p);
    }
    if (parser_match_keyword(p, "struct")) {
        return parse_struct_decl(p);
    }
    if (parser_match_keyword(p, "function")) {
        return parse_function_decl(p);
    }
    return parse_statement(p);
}

static bool parse_use_decl(parser *p) {
    if (!parser_expect_identifier(p, "expected module name")) {
        return false;
    }
    while (parser_match_operator(p, ".")) {
        if (!parser_expect_identifier(p, "expected module name")) {
            return false;
        }
    }
    return parse_statement_terminator(p, "expected ';' after use declaration");
}

static bool parse_struct_decl(parser *p) {
    const token *name_tok = parser_current(p);
    name_vec field_names = {0};
    if (!parser_expect_identifier(p, "expected struct name")) {
        return false;
    }
    if (!parser_declare_token(p, name_tok, SYMBOL_STRUCT, "duplicate struct name")) {
        return false;
    }
    if (!parser_expect_operator(p, "(", "expected '(' after struct name")) {
        return false;
    }
    if (!parse_param_list(p, ")", &field_names)) {
        name_vec_free(&field_names);
        return false;
    }
    if (!parser_expect_operator(p, ")", "expected ')' after struct fields")) {
        name_vec_free(&field_names);
        return false;
    }
    if (!parse_statement_terminator(p, "expected ';' after struct declaration")) {
        name_vec_free(&field_names);
        return false;
    }

    semantic_symbol *sym = parser_lookup_mut(p, name_tok);
    if (sym != NULL) {
        sym->type = semantic_type_named(sym->name, sym->len);
        sym->has_arity = true;
        sym->arity = field_names.len;
        sym->variadic = false;
        sym->has_receiver = false;
        sym->receiver_type = semantic_type_unknown();

        if (sym->param_types != NULL) {
            free(sym->param_types);
            sym->param_types = NULL;
            sym->param_count = 0u;
        }
        if (sym->member_names != NULL) {
            for (size_t i = 0; i < sym->member_count; ++i) {
                free(sym->member_names[i]);
            }
            free(sym->member_names);
            sym->member_names = NULL;
        }
        if (sym->member_name_lens != NULL) {
            free(sym->member_name_lens);
            sym->member_name_lens = NULL;
        }
        sym->member_count = 0u;

        if (field_names.len > 0u) {
            sym->param_types = (semantic_type *)malloc(field_names.len * sizeof(*sym->param_types));
            if (sym->param_types == NULL) {
                name_vec_free(&field_names);
                return parser_fail(p, name_tok, "out of memory");
            }
            sym->member_names = (char **)calloc(field_names.len, sizeof(*sym->member_names));
            sym->member_name_lens = (size_t *)calloc(field_names.len, sizeof(*sym->member_name_lens));
            if (sym->member_names == NULL || sym->member_name_lens == NULL) {
                free(sym->param_types);
                sym->param_types = NULL;
                sym->param_count = 0u;
                free(sym->member_names);
                sym->member_names = NULL;
                free(sym->member_name_lens);
                sym->member_name_lens = NULL;
                name_vec_free(&field_names);
                return parser_fail(p, name_tok, "out of memory");
            }
            sym->param_count = field_names.len;
            sym->member_count = field_names.len;
            for (size_t i = 0; i < field_names.len; ++i) {
                char *field_name = (char *)malloc(field_names.items[i].len + 1u);
                if (field_name == NULL) {
                    for (size_t j = 0; j < i; ++j) {
                        free(sym->member_names[j]);
                    }
                    free(sym->member_names);
                    sym->member_names = NULL;
                    free(sym->member_name_lens);
                    sym->member_name_lens = NULL;
                    sym->member_count = 0u;
                    free(sym->param_types);
                    sym->param_types = NULL;
                    sym->param_count = 0u;
                    name_vec_free(&field_names);
                    return parser_fail(p, name_tok, "out of memory");
                }
                memcpy(field_name, field_names.items[i].name, field_names.items[i].len);
                field_name[field_names.items[i].len] = '\0';
                sym->member_names[i] = field_name;
                sym->member_name_lens[i] = field_names.items[i].len;
                sym->param_types[i] = field_names.items[i].type;
            }
        } else {
            sym->param_types = NULL;
            sym->param_count = 0u;
            sym->member_names = NULL;
            sym->member_name_lens = NULL;
            sym->member_count = 0u;
        }
    }

    name_vec_free(&field_names);
    return true;
}

static bool parse_function_decl(parser *p) {
    name_vec generic_names = {0};
    name_vec param_names = {0};
    semantic_type return_type = semantic_type_unknown();
    semantic_type receiver_type = semantic_type_unknown();
    bool entered_function_context = false;
    semantic_symbol *function_sym = NULL;
    char *method_key = NULL;
    size_t method_key_len = 0u;

    if (parser_match_operator(p, "<")) {
        if (!parse_generic_param_list(p, &generic_names)) {
            name_vec_free(&generic_names);
            return false;
        }
        if (!parser_expect_operator(p, ">", "expected '>' after generic parameter list")) {
            name_vec_free(&generic_names);
            return false;
        }
    }

    bool has_receiver = false;
    if (parser_check_operator(p, "[") || parser_check_operator(p, "(")) {
        has_receiver = true;
    } else if (parser_check_kind(p, TOKEN_IDENTIFIER) && parser_check_operator_n(p, 1u, ".")) {
        has_receiver = true;
    } else if (token_is_type_keyword(parser_current(p)) && parser_check_operator_n(p, 1u, ".")) {
        has_receiver = true;
    }

    if (has_receiver) {
        if (!parse_type(p, &receiver_type)) {
            name_vec_free(&generic_names);
            return false;
        }
        if (!parser_expect_operator(p, ".", "expected '.' after receiver type")) {
            name_vec_free(&generic_names);
            return false;
        }
    }

    const token *function_name = parser_current(p);
    if (!parser_expect_identifier(p, "expected function name")) {
        name_vec_free(&generic_names);
        return false;
    }
    if (!has_receiver) {
        if (!parser_declare_token(p, function_name, SYMBOL_FUNCTION, "duplicate function name")) {
            name_vec_free(&generic_names);
            return false;
        }
        function_sym = parser_lookup_mut(p, function_name);
    } else {
        if (!semantic_build_method_key(
                receiver_type, function_name->begin, function_name->len, &method_key, &method_key_len)) {
            name_vec_free(&generic_names);
            return parser_fail(p, function_name, "out of memory");
        }
        if (!parser_declare_name(p,
                                 method_key,
                                 method_key_len,
                                 SYMBOL_FUNCTION,
                                 function_name->line,
                                 function_name->column,
                                 "duplicate method name")) {
            free(method_key);
            name_vec_free(&generic_names);
            return false;
        }
        token method_tok = {.kind = TOKEN_IDENTIFIER,
                            .begin = method_key,
                            .len = method_key_len,
                            .line = function_name->line,
                            .column = function_name->column,
                            .end_line = function_name->end_line,
                            .end_column = function_name->end_column};
        function_sym = parser_lookup_mut(p, &method_tok);
    }

    if (!parser_expect_operator(p, "(", "expected '(' after function name")) {
        name_vec_free(&generic_names);
        return false;
    }
    if (!parse_param_list(p, ")", &param_names)) {
        free(method_key);
        name_vec_free(&generic_names);
        name_vec_free(&param_names);
        return false;
    }
    if (!parser_expect_operator(p, ")", "expected ')' after parameter list")) {
        free(method_key);
        name_vec_free(&generic_names);
        name_vec_free(&param_names);
        return false;
    }
    if (function_sym != NULL) {
        function_sym->has_arity = true;
        function_sym->arity = param_names.len;
        function_sym->variadic = false;
        function_sym->has_receiver = has_receiver;
        function_sym->receiver_type = has_receiver ? receiver_type : semantic_type_unknown();
        if (function_sym->param_types != NULL) {
            free(function_sym->param_types);
            function_sym->param_types = NULL;
        }
        function_sym->param_count = 0u;
        if (param_names.len > 0u) {
            function_sym->param_types =
                (semantic_type *)malloc(param_names.len * sizeof(*function_sym->param_types));
            if (function_sym->param_types == NULL) {
                free(method_key);
                name_vec_free(&generic_names);
                name_vec_free(&param_names);
                return parser_fail(p, function_name, "out of memory");
            }
            function_sym->param_count = param_names.len;
            for (size_t i = 0; i < param_names.len; ++i) {
                function_sym->param_types[i] = param_names.items[i].type;
            }
        }
    }

    if (!parser_expect_operator(p, "->", "expected '->' after parameter list")) {
        free(method_key);
        name_vec_free(&generic_names);
        name_vec_free(&param_names);
        return false;
    }
    if (!parse_type(p, &return_type)) {
        free(method_key);
        name_vec_free(&generic_names);
        name_vec_free(&param_names);
        return false;
    }
    if (function_sym != NULL) {
        function_sym->type = return_type;
    }

    semantic_type prev_return_type = p->current_function_return_type;
    p->function_depth += 1u;
    p->current_function_return_type = return_type;
    entered_function_context = true;

    bool ok = parse_function_block(p, &generic_names, &param_names, has_receiver, receiver_type);

    if (entered_function_context) {
        p->function_depth -= 1u;
        p->current_function_return_type = prev_return_type;
    }

    free(method_key);
    name_vec_free(&generic_names);
    name_vec_free(&param_names);
    return ok;
}

static bool parse_generic_param_list(parser *p, name_vec *names) {
    if (parser_check_operator(p, ">")) {
        return true;
    }
    for (;;) {
        const token *name_tok = parser_current(p);
        if (!parser_expect_identifier(p, "expected generic parameter name")) {
            return false;
        }
        bool is_value_param = false;
        semantic_type param_type = semantic_type_unknown();
        if (parser_match_operator(p, ":")) {
            is_value_param = true;
            if (!parse_type(p, &param_type)) {
                return false;
            }
        }
        if (names != NULL) {
            if (!name_vec_push(names, name_tok, is_value_param, param_type)) {
                return parser_fail(p, name_tok, "out of memory");
            }
        }
        if (!parser_match_operator(p, ",")) {
            break;
        }
    }
    return true;
}

static bool parse_param_list(parser *p, const char *end_op, name_vec *names) {
    if (parser_check_operator(p, end_op)) {
        return true;
    }
    for (;;) {
        const token *name_tok = parser_current(p);
        semantic_type param_type = semantic_type_unknown();
        if (!parser_expect_identifier(p, "expected parameter name")) {
            return false;
        }
        if (parser_match_operator(p, ":")) {
            if (!parse_type(p, &param_type)) {
                return false;
            }
        }
        if (names != NULL) {
            if (!name_vec_push(names, name_tok, true, param_type)) {
                return parser_fail(p, name_tok, "out of memory");
            }
        }
        if (!parser_match_operator(p, ",")) {
            break;
        }
    }
    return true;
}

static bool parse_function_block(parser *p,
                                 const name_vec *generic_names,
                                 const name_vec *param_names,
                                 bool has_receiver,
                                 semantic_type receiver_type) {
    if (!parser_expect_operator(p, "{", "expected '{'")) {
        return false;
    }

    if (!parser_begin_scope(p)) {
        return parser_fail(p, parser_current(p), "out of memory");
    }

    for (size_t i = 0; i < generic_names->len; ++i) {
        const name_entry *entry = &generic_names->items[i];
        symbol_kind kind = entry->is_value_param ? SYMBOL_OBJECT : SYMBOL_TYPE_PARAM;
        if (!parser_declare_name(p,
                                 entry->name,
                                 entry->len,
                                 kind,
                                 entry->line,
                                 entry->column,
                                 "duplicate generic name")) {
            parser_end_scope(p);
            return false;
        }
        p->symbols[p->symbol_count - 1u].type = entry->type;
    }

    for (size_t i = 0; i < param_names->len; ++i) {
        const name_entry *entry = &param_names->items[i];
        if (!parser_declare_name(p,
                                 entry->name,
                                 entry->len,
                                 SYMBOL_OBJECT,
                                 entry->line,
                                 entry->column,
                                 "duplicate parameter name")) {
            parser_end_scope(p);
            return false;
        }
        p->symbols[p->symbol_count - 1u].type = entry->type;
    }

    if (has_receiver) {
        const token *anchor = parser_current(p);
        size_t line = anchor == NULL ? 0u : anchor->line;
        size_t column = anchor == NULL ? 0u : anchor->column;
        if (!parser_declare_name(
                p, "this", 4u, SYMBOL_OBJECT, line, column, "duplicate receiver name")) {
            parser_end_scope(p);
            return false;
        }
        p->symbols[p->symbol_count - 1u].type = receiver_type;
    }

    while (!parser_is_at_end(p) && !parser_check_operator(p, "}")) {
        if (!parse_statement(p)) {
            parser_end_scope(p);
            return false;
        }
    }

    if (!parser_expect_operator(p, "}", "expected '}' after block")) {
        parser_end_scope(p);
        return false;
    }

    parser_end_scope(p);
    return true;
}

static bool parse_block_statement(parser *p) {
    if (!parser_expect_operator(p, "{", "expected '{'")) {
        return false;
    }

    if (!parser_begin_scope(p)) {
        return parser_fail(p, parser_current(p), "out of memory");
    }

    while (!parser_is_at_end(p) && !parser_check_operator(p, "}")) {
        if (!parse_statement(p)) {
            parser_end_scope(p);
            return false;
        }
    }
    if (!parser_expect_operator(p, "}", "expected '}' after block")) {
        parser_end_scope(p);
        return false;
    }

    parser_end_scope(p);
    return true;
}

static bool parse_statement(parser *p) {
    if (parser_check_operator(p, "{")) {
        return parse_block_statement(p);
    }

    if (parser_match_keyword(p, "let") || parser_match_keyword(p, "const")) {
        return parse_binding_statement(p);
    }
    if (parser_match_keyword(p, "if")) {
        return parse_if_statement(p);
    }
    if (parser_match_keyword(p, "while")) {
        return parse_while_statement(p);
    }
    if (parser_match_keyword(p, "loop")) {
        return parse_loop_statement(p);
    }
    if (parser_match_keyword(p, "for")) {
        return parse_for_statement(p);
    }
    if (parser_match_keyword(p, "return")) {
        return parse_return_statement(p);
    }
    if (parser_match_keyword(p, "break")) {
        if (p->loop_depth == 0u) {
            return parser_fail(p, parser_previous(p), "break is only allowed in loops");
        }
        return parse_statement_terminator(p, "expected ';' after break");
    }
    if (parser_match_keyword(p, "continue")) {
        if (p->loop_depth == 0u) {
            return parser_fail(p, parser_previous(p), "continue is only allowed in loops");
        }
        return parse_statement_terminator(p, "expected ';' after continue");
    }

    return parse_expression_or_assignment_statement(p);
}

static bool parse_binding_statement(parser *p) {
    const token *name_tok = parser_current(p);
    semantic_type declared_type = semantic_type_unknown();
    semantic_type initializer_type = semantic_type_unknown();
    semantic_type final_type = semantic_type_unknown();
    expr_result init_result = {0};
    if (!parser_expect_identifier(p, "expected binding name")) {
        return false;
    }
    bool has_type = false;
    bool has_initializer = false;

    if (parser_match_operator(p, ":")) {
        has_type = true;
        if (!parse_type(p, &declared_type)) {
            return false;
        }
    }
    if (parser_match_operator(p, "=")) {
        has_initializer = true;
        if (!parse_expression(p, &init_result)) {
            return false;
        }
        initializer_type = init_result.type;
    }

    if (!has_type && !has_initializer) {
        return parser_fail(p, name_tok, "binding needs a type or initializer");
    }

    if (has_type && has_initializer &&
        !semantic_type_compatible(declared_type, initializer_type)) {
        return parser_fail_type_mismatch(
            p, name_tok, "binding initializer type mismatch", declared_type, initializer_type);
    }

    if (has_type) {
        final_type = declared_type;
    } else {
        final_type = initializer_type;
    }

    if (!parse_statement_terminator(p, "expected ';' after binding declaration")) {
        return false;
    }

    if (!parser_declare_token(p, name_tok, SYMBOL_OBJECT, NULL)) {
        return false;
    }
    semantic_symbol *sym = parser_lookup_mut(p, name_tok);
    if (sym != NULL) {
        sym->type = final_type;
    }
    return true;
}

static bool parse_if_statement(parser *p) {
    expr_result cond_result = {0};

    if (!parser_expect_operator(p, "(", "expected '(' after if")) {
        return false;
    }
    if (!parse_expression(p, &cond_result)) {
        return false;
    }
    if (!semantic_type_is_unknown(cond_result.type) && !semantic_type_is_bool(cond_result.type)) {
        return parser_fail_type_mismatch(
            p, parser_previous(p), "if condition must be bool", (semantic_type){.kind = TYPE_BOOL}, cond_result.type);
    }
    if (!parser_expect_operator(p, ")", "expected ')' after if condition")) {
        return false;
    }
    if (!parse_statement(p)) {
        return false;
    }
    if (parser_match_keyword(p, "else")) {
        if (!parse_statement(p)) {
            return false;
        }
    }
    return true;
}

static bool parse_while_statement(parser *p) {
    expr_result cond_result = {0};

    if (!parser_expect_operator(p, "(", "expected '(' after while")) {
        return false;
    }
    if (!parse_expression(p, &cond_result)) {
        return false;
    }
    if (!semantic_type_is_unknown(cond_result.type) && !semantic_type_is_bool(cond_result.type)) {
        return parser_fail_type_mismatch(p,
                                         parser_previous(p),
                                         "while condition must be bool",
                                         (semantic_type){.kind = TYPE_BOOL},
                                         cond_result.type);
    }
    if (!parser_expect_operator(p, ")", "expected ')' after while condition")) {
        return false;
    }

    p->loop_depth += 1u;
    bool ok = parse_statement(p);
    p->loop_depth -= 1u;
    return ok;
}

static bool parse_loop_statement(parser *p) {
    p->loop_depth += 1u;
    bool ok = parse_statement(p);
    p->loop_depth -= 1u;
    return ok;
}

static bool parse_for_statement(parser *p) {
    expr_result source_result = {0};
    semantic_type iterator_type = semantic_type_unknown();

    if (!parser_expect_operator(p, "(", "expected '(' after for")) {
        return false;
    }
    const token *iterator_tok = parser_current(p);
    if (!parser_expect_identifier(p, "expected iterator name")) {
        return false;
    }
    if (!parser_expect_operator(p, ":", "expected ':' after iterator name")) {
        return false;
    }
    if (!parse_expression(p, &source_result)) {
        return false;
    }
    if (!semantic_type_is_unknown(source_result.type) && !semantic_type_is_array_or_range(source_result.type)) {
        return parser_fail(p, iterator_tok, "for source expression must be an array or range");
    }
    if (source_result.type.kind == TYPE_RANGE) {
        iterator_type = (semantic_type){.kind = TYPE_NUMBER_LITERAL};
    }
    if (!parser_expect_operator(p, ")", "expected ')' after for header")) {
        return false;
    }

    if (!parser_begin_scope(p)) {
        return parser_fail(p, iterator_tok, "out of memory");
    }

    if (!parser_declare_token(p, iterator_tok, SYMBOL_OBJECT, "duplicate iterator name")) {
        parser_end_scope(p);
        return false;
    }
    semantic_symbol *iterator_sym = parser_lookup_mut(p, iterator_tok);
    if (iterator_sym != NULL) {
        iterator_sym->type = iterator_type;
    }

    p->loop_depth += 1u;
    bool ok = parse_statement(p);
    p->loop_depth -= 1u;

    parser_end_scope(p);
    return ok;
}

static bool parse_return_statement(parser *p) {
    expr_result result = {0};

    if (p->function_depth == 0u) {
        return parser_fail(p, parser_previous(p), "return is only allowed in functions");
    }

    if (parser_match_operator(p, ";")) {
        if (!semantic_type_is_unit(p->current_function_return_type)) {
            return parser_fail(p, parser_previous(p), "return value is required");
        }
        return true;
    }
    if (is_implicit_terminator(p)) {
        if (!semantic_type_is_unit(p->current_function_return_type)) {
            return parser_fail(p, parser_previous(p), "return value is required");
        }
        return true;
    }
    if (!parse_expression(p, &result)) {
        return false;
    }

    if (semantic_type_is_unit(p->current_function_return_type)) {
        return parser_fail(p, parser_previous(p), "this function returns unit");
    }
    if (!semantic_type_compatible(p->current_function_return_type, result.type)) {
        return parser_fail_type_mismatch(p,
                                         parser_previous(p),
                                         "return type mismatch",
                                         p->current_function_return_type,
                                         result.type);
    }

    return parse_statement_terminator(p, "expected ';' after return");
}

static bool parse_expression_or_assignment_statement(parser *p) {
    expr_result lhs = {0};
    if (!parse_expression(p, &lhs)) {
        return false;
    }

    const token *tok = parser_current(p);
    if (tok->kind == TOKEN_OPERATOR && token_is_assignment_operator(tok)) {
        const token *op_tok = tok;
        expr_result rhs = {0};
        parser_advance(p);
        if (!lhs.assignable) {
            return parser_fail(p, op_tok, "left side of assignment is not assignable");
        }
        if (!parse_expression(p, &rhs)) {
            return false;
        }
        if (token_text_eq(op_tok, "=")) {
            if (!semantic_type_compatible(lhs.type, rhs.type)) {
                return parser_fail_type_mismatch(
                    p, op_tok, "assignment type mismatch", lhs.type, rhs.type);
            }
        } else {
            if (!semantic_type_is_unknown(lhs.type) && !semantic_type_is_numeric(lhs.type)) {
                return parser_fail(p, op_tok, "left side of compound assignment must be numeric");
            }
            if (!semantic_type_is_unknown(rhs.type) && !semantic_type_is_numeric(rhs.type)) {
                return parser_fail(p, op_tok, "right side of compound assignment must be numeric");
            }
            if (semantic_type_is_numeric(lhs.type) && semantic_type_is_numeric(rhs.type) &&
                semantic_type_is_unknown(semantic_common_numeric_type(lhs.type, rhs.type))) {
                return parser_fail_type_mismatch(
                    p, op_tok, "compound assignment type mismatch", lhs.type, rhs.type);
            }
        }
        return parse_statement_terminator(p, "expected ';' after assignment");
    }
    return parse_statement_terminator(p, "expected ';' after expression");
}

static bool parse_expression(parser *p, expr_result *out) {
    return parse_logical_or(p, out);
}

static bool parse_logical_or(parser *p, expr_result *out) {
    expr_result lhs = {0};
    if (!parse_logical_and(p, &lhs)) {
        return false;
    }
    while (parser_match_operator(p, "||")) {
        const token *op_tok = parser_previous(p);
        expr_result rhs = {0};
        if (!parse_logical_and(p, &rhs)) {
            return false;
        }
        if (!semantic_type_is_unknown(lhs.type) && !semantic_type_is_bool(lhs.type)) {
            return parser_fail_type_mismatch(
                p, op_tok, "left operand of || must be bool", (semantic_type){.kind = TYPE_BOOL}, lhs.type);
        }
        if (!semantic_type_is_unknown(rhs.type) && !semantic_type_is_bool(rhs.type)) {
            return parser_fail_type_mismatch(
                p, op_tok, "right operand of || must be bool", (semantic_type){.kind = TYPE_BOOL}, rhs.type);
        }
        lhs.type = (semantic_type){.kind = TYPE_BOOL};
        lhs.assignable = false;
        lhs.callee = (expr_meta){0};
    }
    *out = lhs;
    return true;
}

static bool parse_logical_and(parser *p, expr_result *out) {
    expr_result lhs = {0};
    if (!parse_equality(p, &lhs)) {
        return false;
    }
    while (parser_match_operator(p, "&&")) {
        const token *op_tok = parser_previous(p);
        expr_result rhs = {0};
        if (!parse_equality(p, &rhs)) {
            return false;
        }
        if (!semantic_type_is_unknown(lhs.type) && !semantic_type_is_bool(lhs.type)) {
            return parser_fail_type_mismatch(
                p, op_tok, "left operand of && must be bool", (semantic_type){.kind = TYPE_BOOL}, lhs.type);
        }
        if (!semantic_type_is_unknown(rhs.type) && !semantic_type_is_bool(rhs.type)) {
            return parser_fail_type_mismatch(
                p, op_tok, "right operand of && must be bool", (semantic_type){.kind = TYPE_BOOL}, rhs.type);
        }
        lhs.type = (semantic_type){.kind = TYPE_BOOL};
        lhs.assignable = false;
        lhs.callee = (expr_meta){0};
    }
    *out = lhs;
    return true;
}

static bool parse_equality(parser *p, expr_result *out) {
    expr_result lhs = {0};
    if (!parse_comparison(p, &lhs)) {
        return false;
    }
    while (parser_match_operator(p, "==") || parser_match_operator(p, "!=")) {
        const token *op_tok = parser_previous(p);
        expr_result rhs = {0};
        if (!parse_comparison(p, &rhs)) {
            return false;
        }
        if (!semantic_type_is_unknown(lhs.type) && !semantic_type_is_unknown(rhs.type) &&
            !semantic_type_compatible(lhs.type, rhs.type) &&
            !semantic_type_compatible(rhs.type, lhs.type)) {
            return parser_fail_type_mismatch(
                p, op_tok, "equality operand type mismatch", lhs.type, rhs.type);
        }
        lhs.type = (semantic_type){.kind = TYPE_BOOL};
        lhs.assignable = false;
        lhs.callee = (expr_meta){0};
    }
    *out = lhs;
    return true;
}

static bool parse_comparison(parser *p, expr_result *out) {
    expr_result lhs = {0};
    if (!parse_range(p, &lhs)) {
        return false;
    }
    while (parser_match_operator(p, ">") || parser_match_operator(p, "<") ||
           parser_match_operator(p, ">=") || parser_match_operator(p, "<=")) {
        const token *op_tok = parser_previous(p);
        expr_result rhs = {0};
        if (!parse_range(p, &rhs)) {
            return false;
        }
        if (!semantic_type_is_unknown(lhs.type) && !semantic_type_is_numeric(lhs.type)) {
            return parser_fail(p, op_tok, "left operand of comparison must be numeric");
        }
        if (!semantic_type_is_unknown(rhs.type) && !semantic_type_is_numeric(rhs.type)) {
            return parser_fail(p, op_tok, "right operand of comparison must be numeric");
        }
        if (semantic_type_is_numeric(lhs.type) && semantic_type_is_numeric(rhs.type) &&
            semantic_type_is_unknown(semantic_common_numeric_type(lhs.type, rhs.type))) {
            return parser_fail_type_mismatch(
                p, op_tok, "comparison operand type mismatch", lhs.type, rhs.type);
        }
        lhs.type = (semantic_type){.kind = TYPE_BOOL};
        lhs.assignable = false;
        lhs.callee = (expr_meta){0};
    }
    *out = lhs;
    return true;
}

static bool parse_range(parser *p, expr_result *out) {
    expr_result lhs = {0};
    if (!parse_additive(p, &lhs)) {
        return false;
    }
    if (parser_match_operator(p, "..") || parser_match_operator(p, "...")) {
        const token *op_tok = parser_previous(p);
        expr_result end_result = {0};
        if (!parse_additive(p, &end_result)) {
            return false;
        }
        if (!semantic_type_is_unknown(lhs.type) && !semantic_type_is_numeric(lhs.type)) {
            return parser_fail(p, op_tok, "range start must be numeric");
        }
        if (!semantic_type_is_unknown(end_result.type) && !semantic_type_is_numeric(end_result.type)) {
            return parser_fail(p, op_tok, "range end must be numeric");
        }
        if (parser_match_operator(p, "..")) {
            expr_result step_result = {0};
            if (!parse_additive(p, &step_result)) {
                return false;
            }
            if (!semantic_type_is_unknown(step_result.type) && !semantic_type_is_numeric(step_result.type)) {
                return parser_fail(p, parser_previous(p), "range step must be numeric");
            }
        }
        lhs.type = (semantic_type){.kind = TYPE_RANGE};
        lhs.assignable = false;
        lhs.callee = (expr_meta){0};
    }
    *out = lhs;
    return true;
}

static bool parse_additive(parser *p, expr_result *out) {
    expr_result lhs = {0};
    if (!parse_multiplicative(p, &lhs)) {
        return false;
    }
    while (parser_match_operator(p, "+") || parser_match_operator(p, "-")) {
        const token *op_tok = parser_previous(p);
        expr_result rhs = {0};
        if (!parse_multiplicative(p, &rhs)) {
            return false;
        }
        if (!semantic_type_is_unknown(lhs.type) && !semantic_type_is_numeric(lhs.type)) {
            return parser_fail(p, op_tok, "left operand of additive expression must be numeric");
        }
        if (!semantic_type_is_unknown(rhs.type) && !semantic_type_is_numeric(rhs.type)) {
            return parser_fail(p, op_tok, "right operand of additive expression must be numeric");
        }
        semantic_type merged = semantic_common_numeric_type(lhs.type, rhs.type);
        if (semantic_type_is_numeric(lhs.type) && semantic_type_is_numeric(rhs.type) &&
            semantic_type_is_unknown(merged)) {
            return parser_fail_type_mismatch(
                p, op_tok, "additive operand type mismatch", lhs.type, rhs.type);
        }
        lhs.type = merged;
        lhs.assignable = false;
        lhs.callee = (expr_meta){0};
    }
    *out = lhs;
    return true;
}

static bool parse_multiplicative(parser *p, expr_result *out) {
    expr_result lhs = {0};
    if (!parse_unary(p, &lhs)) {
        return false;
    }
    while (parser_match_operator(p, "*") || parser_match_operator(p, "/")) {
        const token *op_tok = parser_previous(p);
        expr_result rhs = {0};
        if (!parse_unary(p, &rhs)) {
            return false;
        }
        if (!semantic_type_is_unknown(lhs.type) && !semantic_type_is_numeric(lhs.type)) {
            return parser_fail(
                p, op_tok, "left operand of multiplicative expression must be numeric");
        }
        if (!semantic_type_is_unknown(rhs.type) && !semantic_type_is_numeric(rhs.type)) {
            return parser_fail(
                p, op_tok, "right operand of multiplicative expression must be numeric");
        }
        semantic_type merged = semantic_common_numeric_type(lhs.type, rhs.type);
        if (semantic_type_is_numeric(lhs.type) && semantic_type_is_numeric(rhs.type) &&
            semantic_type_is_unknown(merged)) {
            return parser_fail_type_mismatch(
                p, op_tok, "multiplicative operand type mismatch", lhs.type, rhs.type);
        }
        lhs.type = merged;
        lhs.assignable = false;
        lhs.callee = (expr_meta){0};
    }
    *out = lhs;
    return true;
}

static bool parse_unary(parser *p, expr_result *out) {
    const token *tok = parser_current(p);
    if (tok->kind == TOKEN_OPERATOR && token_is_unary_operator(tok)) {
        parser_advance(p);
        if (!parse_unary(p, out)) {
            return false;
        }
        if (token_text_eq(tok, "!")) {
            if (!semantic_type_is_unknown(out->type) && !semantic_type_is_bool(out->type)) {
                return parser_fail_type_mismatch(
                    p, tok, "operand of ! must be bool", (semantic_type){.kind = TYPE_BOOL}, out->type);
            }
            out->type = (semantic_type){.kind = TYPE_BOOL};
        } else {
            if (!semantic_type_is_unknown(out->type) && !semantic_type_is_numeric(out->type)) {
                return parser_fail(p, tok, "operand of unary - must be numeric");
            }
        }
        out->assignable = false;
        out->callee = (expr_meta){0};
        return true;
    }
    return parse_postfix(p, out);
}

static bool parse_postfix(parser *p, expr_result *out) {
    expr_result result = {0};
    if (!parse_primary(p, &result)) {
        return false;
    }
    for (;;) {
        if (parser_match_operator(p, "(")) {
            type_vec arg_types = {0};
            if (!parse_argument_list(p, ")", &arg_types)) {
                type_vec_free(&arg_types);
                return false;
            }
            if (!parser_expect_operator(p, ")", "expected ')' after argument list")) {
                type_vec_free(&arg_types);
                return false;
            }

            const semantic_symbol *callee = NULL;
            if ((result.callee.kind == CALLEE_IDENTIFIER || result.callee.kind == CALLEE_MEMBER) &&
                result.callee.symbol != NULL) {
                callee = result.callee.symbol;
            }

            if (callee != NULL) {
                if (callee->kind == SYMBOL_FUNCTION || callee->kind == SYMBOL_STRUCT) {
                    bool arity_mismatch = false;
                    if (callee->has_arity) {
                        if (callee->variadic) {
                            arity_mismatch = arg_types.len < callee->arity;
                        } else {
                            arity_mismatch = arg_types.len != callee->arity;
                        }
                    }
                    if (arity_mismatch) {
                        char msg[256];
                        snprintf(msg,
                                 sizeof(msg),
                                 "argument count mismatch for %.*s",
                                 (int)result.callee.name_tok->len,
                                 result.callee.name_tok->begin);
                        type_vec_free(&arg_types);
                        return parser_fail(p, result.callee.name_tok, msg);
                    }

                    size_t check_count = callee->param_count;
                    if (check_count > arg_types.len) {
                        check_count = arg_types.len;
                    }
                    for (size_t i = 0; i < check_count; ++i) {
                        if (!semantic_type_compatible(callee->param_types[i], arg_types.items[i])) {
                            char msg[256];
                            snprintf(msg,
                                     sizeof(msg),
                                     "argument %zu type mismatch for %.*s",
                                     i + 1u,
                                     (int)result.callee.name_tok->len,
                                     result.callee.name_tok->begin);
                            type_vec_free(&arg_types);
                            return parser_fail_type_mismatch(
                                p, result.callee.name_tok, msg, callee->param_types[i], arg_types.items[i]);
                        }
                    }

                    if (callee->kind == SYMBOL_FUNCTION) {
                        result.type = callee->type;
                    } else {
                        result.type = semantic_type_named(callee->name, callee->len);
                    }
                } else {
                    char msg[256];
                    snprintf(msg,
                             sizeof(msg),
                             "%.*s is not callable",
                             (int)result.callee.name_tok->len,
                             result.callee.name_tok->begin);
                    type_vec_free(&arg_types);
                    return parser_fail(p, result.callee.name_tok, msg);
                }
            } else if (result.callee.kind == CALLEE_MEMBER) {
                if (!semantic_type_is_unknown(result.type)) {
                    type_vec_free(&arg_types);
                    return parser_fail(p, result.callee.name_tok, "member is not callable");
                }
                result.type = semantic_type_unknown();
            } else {
                type_vec_free(&arg_types);
                return parser_fail(p, parser_previous(p), "expression is not callable");
            }

            type_vec_free(&arg_types);
            result.assignable = false;
            result.callee = (expr_meta){0};
            continue;
        }
        if (parser_match_operator(p, ".")) {
            semantic_type receiver_type = result.type;
            const token *member_tok = parser_current(p);
            if (!parser_expect_identifier(p, "expected member name after '.'")) {
                return false;
            }

            bool has_field = false;
            semantic_type field_type = semantic_type_unknown();
            if (receiver_type.kind == TYPE_NAMED && receiver_type.name != NULL) {
                const semantic_symbol *struct_sym =
                    parser_lookup_name_kind(p, receiver_type.name, receiver_type.name_len, SYMBOL_STRUCT);
                if (struct_sym != NULL) {
                    has_field = semantic_lookup_struct_field(struct_sym, member_tok, &field_type);
                }
            }
            const semantic_symbol *method_sym =
                semantic_lookup_method_symbol(p, receiver_type, member_tok);

            if (has_field) {
                result.type = field_type;
            } else {
                result.type = semantic_type_unknown();
            }
            result.assignable = has_field;
            result.callee.kind = CALLEE_MEMBER;
            result.callee.name_tok = member_tok;
            result.callee.symbol = method_sym;
            continue;
        }
        break;
    }
    *out = result;
    return true;
}

static bool parse_primary(parser *p, expr_result *out) {
    out->type = semantic_type_unknown();
    out->assignable = false;
    out->callee = (expr_meta){0};

    if (parser_match_kind(p, TOKEN_NUMBER)) {
        out->type = (semantic_type){.kind = TYPE_NUMBER_LITERAL};
        return true;
    }
    if (parser_match_kind(p, TOKEN_STRING)) {
        out->type = (semantic_type){.kind = TYPE_STRING};
        return true;
    }
    if (parser_match_kind(p, TOKEN_CHAR)) {
        out->type = (semantic_type){.kind = TYPE_CHAR};
        return true;
    }

    if (parser_match_kind(p, TOKEN_IDENTIFIER)) {
        const token *id_tok = parser_previous(p);
        if (!parser_require_identifier(p, id_tok)) {
            return false;
        }
        const semantic_symbol *sym = parser_lookup(p, id_tok);
        out->type = semantic_type_of_symbol(sym);
        out->assignable = sym != NULL && sym->kind == SYMBOL_OBJECT;
        out->callee.kind = CALLEE_IDENTIFIER;
        out->callee.name_tok = id_tok;
        out->callee.symbol = sym;
        return true;
    }

    if (parser_match_keyword(p, "true") || parser_match_keyword(p, "false")) {
        out->type = (semantic_type){.kind = TYPE_BOOL};
        return true;
    }
    if (parser_match_keyword(p, "this")) {
        const token *this_tok = parser_previous(p);
        const semantic_symbol *this_sym = parser_lookup(p, this_tok);
        if (this_sym != NULL) {
            out->type = this_sym->type;
        }
        return true;
    }
    if (parser_match_keyword(p, "null")) {
        out->type = (semantic_type){.kind = TYPE_NULL};
        return true;
    }

    if (parser_match_operator(p, "(")) {
        if (parser_match_operator(p, ")")) {
            out->type = semantic_type_unit();
            return true;
        }
        expr_result first = {0};
        if (!parse_expression(p, &first)) {
            return false;
        }
        if (parser_match_operator(p, ",")) {
            do {
                expr_result item = {0};
                if (!parse_expression(p, &item)) {
                    return false;
                }
            } while (parser_match_operator(p, ","));
            if (!parser_expect_operator(p, ")", "expected ')' after tuple")) {
                return false;
            }
            out->type = (semantic_type){.kind = TYPE_TUPLE};
            out->assignable = false;
            out->callee = (expr_meta){0};
            return true;
        }
        if (!parser_expect_operator(p, ")", "expected ')' after expression")) {
            return false;
        }
        *out = first;
        return true;
    }

    if (parser_match_operator(p, "[")) {
        if (!parser_check_operator(p, "]")) {
            expr_result item = {0};
            if (!parse_expression(p, &item)) {
                return false;
            }
            while (parser_match_operator(p, ",")) {
                if (!parse_expression(p, &item)) {
                    return false;
                }
            }
        }
        if (!parser_expect_operator(p, "]", "expected ']' after array literal")) {
            return false;
        }
        out->type = (semantic_type){.kind = TYPE_ARRAY};
        out->assignable = false;
        out->callee = (expr_meta){0};
        return true;
    }

    return parser_fail(p, parser_current(p), "expected expression");
}

static bool parse_argument_list(parser *p, const char *end_op, type_vec *arg_types) {
    if (parser_check_operator(p, end_op)) {
        return true;
    }
    expr_result arg = {0};
    if (!parse_expression(p, &arg)) {
        return false;
    }
    if (arg_types != NULL && !type_vec_push(arg_types, arg.type)) {
        return parser_fail(p, parser_current(p), "out of memory");
    }
    while (parser_match_operator(p, ",")) {
        if (!parse_expression(p, &arg)) {
            return false;
        }
        if (arg_types != NULL && !type_vec_push(arg_types, arg.type)) {
            return parser_fail(p, parser_current(p), "out of memory");
        }
    }
    return true;
}

static bool parse_type(parser *p, semantic_type *out) {
    semantic_type parsed = semantic_type_unknown();

    if (parser_match_operator(p, "[")) {
        semantic_type element_type = semantic_type_unknown();
        if (!parse_type(p, &element_type)) {
            return false;
        }
        if (parser_match_operator(p, ":")) {
            if (!parse_type_length(p)) {
                return false;
            }
            if (!parser_expect_operator(p, "]", "expected ']' after array type")) {
                return false;
            }
            parsed = (semantic_type){.kind = TYPE_ARRAY};
            if (out != NULL) {
                *out = parsed;
            }
            return true;
        }
        if (parser_match_operator(p, ",")) {
            do {
                semantic_type tuple_item = semantic_type_unknown();
                if (!parse_type(p, &tuple_item)) {
                    return false;
                }
            } while (parser_match_operator(p, ","));
            if (!parser_expect_operator(p, "]", "expected ']' after receiver type")) {
                return false;
            }
            parsed = (semantic_type){.kind = TYPE_TUPLE};
            if (out != NULL) {
                *out = parsed;
            }
            return true;
        }
        if (!parser_expect_operator(p, "]", "expected ']' after array type")) {
            return false;
        }
        parsed = (semantic_type){.kind = TYPE_ARRAY};
        if (out != NULL) {
            *out = parsed;
        }
        return true;
    }

    if (parser_match_operator(p, "(")) {
        if (parser_match_operator(p, ")")) {
            parsed = semantic_type_unit();
            if (out != NULL) {
                *out = parsed;
            }
            return true;
        }
        semantic_type first = semantic_type_unknown();
        if (!parse_type(p, &first)) {
            return false;
        }
        if (parser_match_operator(p, ",")) {
            do {
                semantic_type tuple_item = semantic_type_unknown();
                if (!parse_type(p, &tuple_item)) {
                    return false;
                }
            } while (parser_match_operator(p, ","));
            if (!parser_expect_operator(p, ")", "expected ')' after tuple type")) {
                return false;
            }
            parsed = (semantic_type){.kind = TYPE_TUPLE};
        } else {
            if (!parser_expect_operator(p, ")", "expected ')' after grouped type")) {
                return false;
            }
            parsed = first;
        }
        if (out != NULL) {
            *out = parsed;
        }
        return true;
    }

    if (parser_match_kind(p, TOKEN_IDENTIFIER)) {
        const token *tok = parser_previous(p);
        parsed = semantic_type_named(tok->begin, tok->len);
        if (out != NULL) {
            *out = parsed;
        }
        return true;
    }

    if (parser_check_kind(p, TOKEN_KEYWORD) && token_is_type_keyword(parser_current(p))) {
        const token *tok = parser_current(p);
        parser_advance(p);
        parsed = semantic_type_from_type_keyword(tok);
        if (out != NULL) {
            *out = parsed;
        }
        return true;
    }

    return parser_fail(p, parser_current(p), "expected type");
}

static bool parse_type_length(parser *p) {
    if (parser_match_kind(p, TOKEN_NUMBER)) {
        return true;
    }
    if (parser_match_kind(p, TOKEN_IDENTIFIER)) {
        return parser_require_identifier(p, parser_previous(p));
    }
    return parser_fail(p, parser_current(p), "expected array length");
}

static bool parse_statement_terminator(parser *p, const char *message) {
    if (parser_match_operator(p, ";")) {
        return true;
    }
    if (is_implicit_terminator(p)) {
        return true;
    }
    return parser_fail(p, parser_current(p), message);
}

static bool is_implicit_terminator(const parser *p) {
    if (parser_check_kind(p, TOKEN_EOF) || parser_check_operator(p, "}")) {
        return true;
    }
    const token *prev = parser_previous(p);
    const token *cur = parser_current(p);
    if (prev == NULL || cur == NULL) {
        return false;
    }
    return cur->line > prev->end_line;
}

static bool parser_match_kind(parser *p, token_kind kind) {
    if (!parser_check_kind(p, kind)) {
        return false;
    }
    parser_advance(p);
    return true;
}

static bool parser_match_keyword(parser *p, const char *keyword) {
    if (!parser_check_keyword(p, keyword)) {
        return false;
    }
    parser_advance(p);
    return true;
}

static bool parser_match_operator(parser *p, const char *op) {
    if (!parser_check_operator(p, op)) {
        return false;
    }
    parser_advance(p);
    return true;
}

static bool parser_expect_identifier(parser *p, const char *message) {
    if (!parser_match_kind(p, TOKEN_IDENTIFIER)) {
        return parser_fail(p, parser_current(p), message);
    }
    return true;
}

static bool parser_expect_operator(parser *p, const char *op, const char *message) {
    if (!parser_match_operator(p, op)) {
        return parser_fail(p, parser_current(p), message);
    }
    return true;
}

static bool parser_check_kind(const parser *p, token_kind kind) {
    const token *tok = parser_current(p);
    return tok != NULL && tok->kind == kind;
}

static bool parser_check_keyword(const parser *p, const char *keyword) {
    const token *tok = parser_current(p);
    return tok != NULL && tok->kind == TOKEN_KEYWORD && token_text_eq(tok, keyword);
}

static bool parser_check_operator(const parser *p, const char *op) {
    const token *tok = parser_current(p);
    return tok != NULL && tok->kind == TOKEN_OPERATOR && token_text_eq(tok, op);
}

static bool parser_check_operator_n(const parser *p, size_t n, const char *op) {
    const token *tok = parser_peek_n(p, n);
    return tok != NULL && tok->kind == TOKEN_OPERATOR && token_text_eq(tok, op);
}

static const token *parser_current(const parser *p) {
    if (p->index >= p->count) {
        return NULL;
    }
    return &p->tokens[p->index];
}

static const token *parser_previous(const parser *p) {
    if (p->index == 0u || p->index > p->count) {
        return NULL;
    }
    return &p->tokens[p->index - 1u];
}

static const token *parser_peek_n(const parser *p, size_t n) {
    size_t idx = p->index + n;
    if (idx >= p->count) {
        return NULL;
    }
    return &p->tokens[idx];
}

static bool parser_is_at_end(const parser *p) {
    return parser_check_kind(p, TOKEN_EOF);
}

static void parser_advance(parser *p) {
    if (!parser_is_at_end(p)) {
        p->index += 1u;
    }
}

static bool parser_fail(parser *p, const token *where, const char *message) {
    if (where == NULL) {
        set_error(p->error, p->file_path, 0u, 0u, message);
    } else {
        set_error(p->error, p->file_path, where->line, where->column, message);
    }
    return false;
}

static bool token_text_eq(const token *tok, const char *text) {
    size_t n = strlen(text);
    return tok->len == n && strncmp(tok->begin, text, n) == 0;
}

static bool token_is_type_keyword(const token *tok) {
    if (tok == NULL || tok->kind != TOKEN_KEYWORD) {
        return false;
    }
    for (size_t i = 0; i < sizeof(k_type_keywords) / sizeof(k_type_keywords[0]); ++i) {
        if (token_text_eq(tok, k_type_keywords[i])) {
            return true;
        }
    }
    return false;
}

static bool token_is_assignment_operator(const token *tok) {
    if (tok == NULL || tok->kind != TOKEN_OPERATOR) {
        return false;
    }
    return token_text_eq(tok, "=") || token_text_eq(tok, "+=") || token_text_eq(tok, "-=") ||
           token_text_eq(tok, "*=") || token_text_eq(tok, "/=");
}

static bool token_is_unary_operator(const token *tok) {
    if (tok == NULL || tok->kind != TOKEN_OPERATOR) {
        return false;
    }
    return token_text_eq(tok, "-") || token_text_eq(tok, "!");
}

static bool parser_begin_scope(parser *p) {
    if (p->scope_count == p->scope_cap) {
        size_t next = p->scope_cap == 0u ? 8u : p->scope_cap * 2u;
        size_t *mem = (size_t *)realloc(p->scope_marks, next * sizeof(*mem));
        if (mem == NULL) {
            return false;
        }
        p->scope_marks = mem;
        p->scope_cap = next;
    }
    p->scope_marks[p->scope_count++] = p->symbol_count;
    return true;
}

static void parser_end_scope(parser *p) {
    if (p->scope_count == 0u) {
        return;
    }
    size_t begin = p->scope_marks[p->scope_count - 1u];
    while (p->symbol_count > begin) {
        p->symbol_count -= 1u;
        free(p->symbols[p->symbol_count].param_types);
        p->symbols[p->symbol_count].param_types = NULL;
        p->symbols[p->symbol_count].param_count = 0u;
        if (p->symbols[p->symbol_count].member_names != NULL) {
            for (size_t i = 0; i < p->symbols[p->symbol_count].member_count; ++i) {
                free(p->symbols[p->symbol_count].member_names[i]);
            }
        }
        free(p->symbols[p->symbol_count].member_names);
        p->symbols[p->symbol_count].member_names = NULL;
        free(p->symbols[p->symbol_count].member_name_lens);
        p->symbols[p->symbol_count].member_name_lens = NULL;
        p->symbols[p->symbol_count].member_count = 0u;
        free(p->symbols[p->symbol_count].name);
        p->symbols[p->symbol_count].name = NULL;
    }
    p->scope_count -= 1u;
}

static bool parser_declare_token(parser *p, const token *tok, symbol_kind kind, const char *duplicate_message) {
    return parser_declare_name(p, tok->begin, tok->len, kind, tok->line, tok->column, duplicate_message);
}

static bool parser_declare_name(parser *p,
                                const char *name,
                                size_t len,
                                symbol_kind kind,
                                size_t line,
                                size_t column,
                                const char *duplicate_message) {
    if (p->scope_count == 0u) {
        if (!parser_begin_scope(p)) {
            set_error(p->error, p->file_path, line, column, "out of memory");
            return false;
        }
    }

    size_t begin = p->scope_marks[p->scope_count - 1u];
    if (duplicate_message != NULL) {
        for (size_t i = begin; i < p->symbol_count; ++i) {
            if (parser_name_equals(&p->symbols[i], name, len)) {
                set_error(p->error, p->file_path, line, column, duplicate_message);
                return false;
            }
        }
    }

    if (p->symbol_count == p->symbol_cap) {
        size_t next = p->symbol_cap == 0u ? 32u : p->symbol_cap * 2u;
        semantic_symbol *mem = (semantic_symbol *)realloc(p->symbols, next * sizeof(*mem));
        if (mem == NULL) {
            set_error(p->error, p->file_path, line, column, "out of memory");
            return false;
        }
        p->symbols = mem;
        p->symbol_cap = next;
    }

    char *copied = (char *)malloc(len + 1u);
    if (copied == NULL) {
        set_error(p->error, p->file_path, line, column, "out of memory");
        return false;
    }
    memcpy(copied, name, len);
    copied[len] = '\0';

    p->symbols[p->symbol_count++] =
        (semantic_symbol){.name = copied,
                          .len = len,
                          .kind = kind,
                          .scope_depth = p->scope_count - 1u,
                          .type = semantic_type_unknown(),
                          .param_types = NULL,
                          .param_count = 0u,
                          .member_names = NULL,
                          .member_name_lens = NULL,
                          .member_count = 0u,
                          .has_receiver = false,
                          .receiver_type = semantic_type_unknown(),
                          .arity = 0u,
                          .has_arity = false,
                          .variadic = false};
    return true;
}

static const semantic_symbol *parser_lookup(parser *p, const token *tok) {
    for (size_t i = p->symbol_count; i > 0u; --i) {
        const semantic_symbol *sym = &p->symbols[i - 1u];
        if (parser_name_equals(sym, tok->begin, tok->len)) {
            return sym;
        }
    }
    return NULL;
}

static semantic_symbol *parser_lookup_mut(parser *p, const token *tok) {
    for (size_t i = p->symbol_count; i > 0u; --i) {
        semantic_symbol *sym = &p->symbols[i - 1u];
        if (parser_name_equals(sym, tok->begin, tok->len)) {
            return sym;
        }
    }
    return NULL;
}

static const semantic_symbol *parser_lookup_name_kind(parser *p,
                                                      const char *name,
                                                      size_t len,
                                                      symbol_kind kind) {
    for (size_t i = p->symbol_count; i > 0u; --i) {
        const semantic_symbol *sym = &p->symbols[i - 1u];
        if (sym->kind == kind && parser_name_equals(sym, name, len)) {
            return sym;
        }
    }
    return NULL;
}

static bool parser_name_equals(const semantic_symbol *sym, const char *name, size_t len) {
    return sym->len == len && strncmp(sym->name, name, len) == 0;
}

static bool parser_require_identifier(parser *p, const token *tok) {
    if (parser_is_builtin_name(tok)) {
        return true;
    }

    const semantic_symbol *sym = parser_lookup(p, tok);
    if (sym != NULL) {
        return true;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "unknown identifier: %.*s", (int)tok->len, tok->begin);
    return parser_fail(p, tok, msg);
}

static bool parser_is_builtin_name(const token *tok) {
    return token_text_eq(tok, "print");
}

static void parser_cleanup(parser *p) {
    while (p->scope_count > 0u) {
        parser_end_scope(p);
    }

    free(p->symbols);
    p->symbols = NULL;
    p->symbol_count = 0u;
    p->symbol_cap = 0u;

    free(p->scope_marks);
    p->scope_marks = NULL;
    p->scope_count = 0u;
    p->scope_cap = 0u;
}

static bool name_vec_push(name_vec *vec, const token *tok, bool is_value_param, semantic_type type) {
    if (vec->len == vec->cap) {
        size_t next = vec->cap == 0u ? 8u : vec->cap * 2u;
        name_entry *mem = (name_entry *)realloc(vec->items, next * sizeof(*mem));
        if (mem == NULL) {
            return false;
        }
        vec->items = mem;
        vec->cap = next;
    }

    char *copied = (char *)malloc(tok->len + 1u);
    if (copied == NULL) {
        return false;
    }
    memcpy(copied, tok->begin, tok->len);
    copied[tok->len] = '\0';

    vec->items[vec->len++] = (name_entry){
        .name = copied,
        .len = tok->len,
        .line = tok->line,
        .column = tok->column,
        .is_value_param = is_value_param,
        .type = type,
    };
    return true;
}

static void name_vec_free(name_vec *vec) {
    for (size_t i = 0; i < vec->len; ++i) {
        free(vec->items[i].name);
        vec->items[i].name = NULL;
    }
    free(vec->items);
    vec->items = NULL;
    vec->len = 0u;
    vec->cap = 0u;
}

static bool type_vec_push(type_vec *vec, semantic_type type) {
    if (vec->len == vec->cap) {
        size_t next = vec->cap == 0u ? 8u : vec->cap * 2u;
        semantic_type *mem = (semantic_type *)realloc(vec->items, next * sizeof(*mem));
        if (mem == NULL) {
            return false;
        }
        vec->items = mem;
        vec->cap = next;
    }
    vec->items[vec->len++] = type;
    return true;
}

static void type_vec_free(type_vec *vec) {
    free(vec->items);
    vec->items = NULL;
    vec->len = 0u;
    vec->cap = 0u;
}

static semantic_type semantic_type_unknown(void) {
    return (semantic_type){.kind = TYPE_UNKNOWN, .name = NULL, .name_len = 0u};
}

static semantic_type semantic_type_unit(void) {
    return (semantic_type){.kind = TYPE_UNIT, .name = NULL, .name_len = 0u};
}

static semantic_type semantic_type_named(const char *name, size_t len) {
    return (semantic_type){.kind = TYPE_NAMED, .name = name, .name_len = len};
}

static semantic_type semantic_type_of_symbol(const semantic_symbol *sym) {
    if (sym == NULL) {
        return semantic_type_unknown();
    }
    if (sym->kind == SYMBOL_STRUCT) {
        return semantic_type_named(sym->name, sym->len);
    }
    return sym->type;
}

static bool semantic_type_is_unknown(semantic_type type) {
    return type.kind == TYPE_UNKNOWN;
}

static bool semantic_type_is_unit(semantic_type type) {
    return type.kind == TYPE_UNIT;
}

static bool semantic_type_is_bool(semantic_type type) {
    return type.kind == TYPE_BOOL;
}

static bool semantic_type_is_numeric(semantic_type type) {
    switch (type.kind) {
        case TYPE_NUMBER_LITERAL:
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64:
        case TYPE_F32:
        case TYPE_F64:
            return true;
        default:
            return false;
    }
}

static bool semantic_type_is_array_or_range(semantic_type type) {
    return type.kind == TYPE_ARRAY || type.kind == TYPE_RANGE;
}

static bool semantic_type_equal(semantic_type lhs, semantic_type rhs) {
    if (lhs.kind != rhs.kind) {
        return false;
    }
    if (lhs.kind == TYPE_NAMED) {
        return lhs.name_len == rhs.name_len && lhs.name != NULL && rhs.name != NULL &&
               strncmp(lhs.name, rhs.name, lhs.name_len) == 0;
    }
    return true;
}

static bool semantic_type_compatible(semantic_type expected, semantic_type actual) {
    if (semantic_type_is_unknown(expected) || semantic_type_is_unknown(actual)) {
        return true;
    }
    if (semantic_type_equal(expected, actual)) {
        return true;
    }
    if (semantic_type_is_numeric(expected) && actual.kind == TYPE_NUMBER_LITERAL) {
        return true;
    }
    if (expected.kind == TYPE_NUMBER_LITERAL && semantic_type_is_numeric(actual)) {
        return true;
    }
    if (expected.kind == TYPE_ARRAY && actual.kind == TYPE_RANGE) {
        return true;
    }
    if (expected.kind == TYPE_NAMED && actual.kind == TYPE_TUPLE) {
        return true;
    }
    if ((expected.kind == TYPE_NAMED || expected.kind == TYPE_ARRAY || expected.kind == TYPE_TUPLE) &&
        actual.kind == TYPE_NULL) {
        return true;
    }
    return false;
}

static semantic_type semantic_type_from_type_keyword(const token *tok) {
    if (token_text_eq(tok, "i8")) {
        return (semantic_type){.kind = TYPE_I8};
    }
    if (token_text_eq(tok, "i16")) {
        return (semantic_type){.kind = TYPE_I16};
    }
    if (token_text_eq(tok, "i32")) {
        return (semantic_type){.kind = TYPE_I32};
    }
    if (token_text_eq(tok, "i64")) {
        return (semantic_type){.kind = TYPE_I64};
    }
    if (token_text_eq(tok, "u8")) {
        return (semantic_type){.kind = TYPE_U8};
    }
    if (token_text_eq(tok, "u16")) {
        return (semantic_type){.kind = TYPE_U16};
    }
    if (token_text_eq(tok, "u32")) {
        return (semantic_type){.kind = TYPE_U32};
    }
    if (token_text_eq(tok, "u64")) {
        return (semantic_type){.kind = TYPE_U64};
    }
    if (token_text_eq(tok, "f32")) {
        return (semantic_type){.kind = TYPE_F32};
    }
    if (token_text_eq(tok, "f64")) {
        return (semantic_type){.kind = TYPE_F64};
    }
    if (token_text_eq(tok, "bool")) {
        return (semantic_type){.kind = TYPE_BOOL};
    }
    if (token_text_eq(tok, "char")) {
        return (semantic_type){.kind = TYPE_CHAR};
    }
    return semantic_type_unknown();
}

static semantic_type semantic_common_numeric_type(semantic_type lhs, semantic_type rhs) {
    if (semantic_type_is_unknown(lhs)) {
        if (semantic_type_is_numeric(rhs)) {
            return rhs;
        }
        return semantic_type_unknown();
    }
    if (semantic_type_is_unknown(rhs)) {
        if (semantic_type_is_numeric(lhs)) {
            return lhs;
        }
        return semantic_type_unknown();
    }
    if (lhs.kind == TYPE_NUMBER_LITERAL && rhs.kind == TYPE_NUMBER_LITERAL) {
        return lhs;
    }
    if (lhs.kind == TYPE_NUMBER_LITERAL && semantic_type_is_numeric(rhs)) {
        return rhs;
    }
    if (rhs.kind == TYPE_NUMBER_LITERAL && semantic_type_is_numeric(lhs)) {
        return lhs;
    }
    if (semantic_type_is_numeric(lhs) && semantic_type_is_numeric(rhs) && lhs.kind == rhs.kind) {
        return lhs;
    }
    return semantic_type_unknown();
}

static void semantic_type_format(semantic_type type, char *buf, size_t buf_size) {
    const char *name = "unknown";
    switch (type.kind) {
        case TYPE_UNKNOWN:
            name = "unknown";
            break;
        case TYPE_UNIT:
            name = "()";
            break;
        case TYPE_BOOL:
            name = "bool";
            break;
        case TYPE_CHAR:
            name = "char";
            break;
        case TYPE_STRING:
            name = "string";
            break;
        case TYPE_NUMBER_LITERAL:
            name = "number";
            break;
        case TYPE_I8:
            name = "i8";
            break;
        case TYPE_I16:
            name = "i16";
            break;
        case TYPE_I32:
            name = "i32";
            break;
        case TYPE_I64:
            name = "i64";
            break;
        case TYPE_U8:
            name = "u8";
            break;
        case TYPE_U16:
            name = "u16";
            break;
        case TYPE_U32:
            name = "u32";
            break;
        case TYPE_U64:
            name = "u64";
            break;
        case TYPE_F32:
            name = "f32";
            break;
        case TYPE_F64:
            name = "f64";
            break;
        case TYPE_NAMED:
            if (type.name != NULL) {
                snprintf(buf, buf_size, "%.*s", (int)type.name_len, type.name);
                return;
            }
            name = "<named>";
            break;
        case TYPE_ARRAY:
            name = "array";
            break;
        case TYPE_TUPLE:
            name = "tuple";
            break;
        case TYPE_RANGE:
            name = "range";
            break;
        case TYPE_NULL:
            name = "null";
            break;
    }
    snprintf(buf, buf_size, "%s", name);
}

static bool semantic_build_method_key(semantic_type receiver_type,
                                      const char *member_name,
                                      size_t member_len,
                                      char **out_key,
                                      size_t *out_len) {
    const char *receiver_name = NULL;
    size_t receiver_len = 0u;
    char receiver_buf[64];

    if (receiver_type.kind == TYPE_NAMED && receiver_type.name != NULL) {
        receiver_name = receiver_type.name;
        receiver_len = receiver_type.name_len;
    } else {
        semantic_type_format(receiver_type, receiver_buf, sizeof(receiver_buf));
        receiver_name = receiver_buf;
        receiver_len = strlen(receiver_buf);
    }

    size_t total_len = receiver_len + 1u + member_len;
    char *key = (char *)malloc(total_len + 1u);
    if (key == NULL) {
        return false;
    }
    memcpy(key, receiver_name, receiver_len);
    key[receiver_len] = '.';
    memcpy(key + receiver_len + 1u, member_name, member_len);
    key[total_len] = '\0';

    if (out_key != NULL) {
        *out_key = key;
    } else {
        free(key);
    }
    if (out_len != NULL) {
        *out_len = total_len;
    }
    return true;
}

static const semantic_symbol *semantic_lookup_method_symbol(parser *p,
                                                            semantic_type receiver_type,
                                                            const token *member_tok) {
    if (semantic_type_is_unknown(receiver_type) || member_tok == NULL) {
        return NULL;
    }
    char *key = NULL;
    size_t key_len = 0u;
    if (!semantic_build_method_key(
            receiver_type, member_tok->begin, member_tok->len, &key, &key_len)) {
        return NULL;
    }
    const semantic_symbol *method = parser_lookup_name_kind(p, key, key_len, SYMBOL_FUNCTION);
    free(key);
    if (method != NULL && method->has_receiver) {
        return method;
    }
    return NULL;
}

static bool semantic_lookup_struct_field(const semantic_symbol *struct_symbol,
                                         const token *member_tok,
                                         semantic_type *out_type) {
    if (struct_symbol == NULL || struct_symbol->kind != SYMBOL_STRUCT || member_tok == NULL) {
        return false;
    }
    if (struct_symbol->member_names == NULL || struct_symbol->member_name_lens == NULL) {
        return false;
    }
    for (size_t i = 0; i < struct_symbol->member_count; ++i) {
        if (struct_symbol->member_name_lens[i] == member_tok->len &&
            strncmp(struct_symbol->member_names[i], member_tok->begin, member_tok->len) == 0) {
            if (out_type != NULL) {
                if (i < struct_symbol->param_count) {
                    *out_type = struct_symbol->param_types[i];
                } else {
                    *out_type = semantic_type_unknown();
                }
            }
            return true;
        }
    }
    return false;
}

static bool parser_fail_type_mismatch(parser *p,
                                      const token *where,
                                      const char *context,
                                      semantic_type expected,
                                      semantic_type actual) {
    char expected_buf[64];
    char actual_buf[64];
    char msg[256];
    semantic_type_format(expected, expected_buf, sizeof(expected_buf));
    semantic_type_format(actual, actual_buf, sizeof(actual_buf));
    if (context != NULL && context[0] != '\0') {
        snprintf(msg, sizeof(msg), "%s: expected %s, got %s", context, expected_buf, actual_buf);
    } else {
        snprintf(msg, sizeof(msg), "type mismatch: expected %s, got %s", expected_buf, actual_buf);
    }
    return parser_fail(p, where, msg);
}

static bool token_vec_push(token_vec *vec, token item) {
    if (vec->len == vec->cap) {
        size_t next = vec->cap == 0u ? 64u : vec->cap * 2u;
        token *mem = (token *)realloc(vec->items, next * sizeof(*mem));
        if (mem == NULL) {
            return false;
        }
        vec->items = mem;
        vec->cap = next;
    }
    vec->items[vec->len++] = item;
    return true;
}

static void token_vec_free(token_vec *vec) {
    free(vec->items);
    vec->items = NULL;
    vec->len = 0u;
    vec->cap = 0u;
}

static void set_error(mdr_error *error,
                      const char *file_path,
                      size_t line,
                      size_t column,
                      const char *message) {
    if (error == NULL) {
        return;
    }
    error->file_path = file_path;
    error->line = line;
    error->column = column;
    snprintf(error->message, sizeof(error->message), "%s", message);
}

static const char *token_kind_name(token_kind kind) {
    switch (kind) {
        case TOKEN_EOF:
            return "eof";
        case TOKEN_IDENTIFIER:
            return "identifier";
        case TOKEN_NUMBER:
            return "number";
        case TOKEN_STRING:
            return "string";
        case TOKEN_CHAR:
            return "char";
        case TOKEN_KEYWORD:
            return "keyword";
        case TOKEN_OPERATOR:
            return "operator";
    }
    return "unknown";
}
