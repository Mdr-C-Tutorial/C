#ifndef MDR_MINI_INTERNAL_H
#define MDR_MINI_INTERNAL_H

#include "../include/mdr_mini.h"

#include "ast.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    MINI_TOKEN_EOF = 0,
    MINI_TOKEN_INT,
    MINI_TOKEN_IDENTIFIER,
    MINI_TOKEN_KEYWORD,
    MINI_TOKEN_OPERATOR
} mini_token_kind;

typedef struct {
    mini_token_kind kind;
    const char *begin;
    size_t len;
    size_t line;
    size_t column;
    size_t end_line;
    size_t end_column;
    int64_t int_value;
} mini_token;

bool mini_lex(const char *source, mini_token **out_tokens, size_t *out_count, mdr_mini_error *error);
void mini_lex_free(mini_token *tokens);

typedef struct {
    mini_token *tokens;
    size_t count;
    size_t index;
    arena *a;
    mdr_mini_error *error;
    size_t loop_depth;
} mini_parser;

bool mini_parse(const char *source, arena *a, node **out_program, mdr_mini_error *error);

bool mdr_buffer_append(mdr_buffer *buffer, const char *text, size_t len);
bool mdr_buffer_append_str(mdr_buffer *buffer, const char *text);
bool mdr_buffer_appendf(mdr_buffer *buffer, const char *format, ...);
void mini_set_error(mdr_mini_error *error,
                    bool runtime,
                    size_t line,
                    size_t column,
                    const char *format,
                    ...);

typedef enum {
    MINI_VAL_INT = 0,
    MINI_VAL_BOOL,
    MINI_VAL_RANGE
} mini_value_kind;

typedef struct {
    int64_t start;
    int64_t end;
    int64_t step;
    bool inclusive;
} mini_range;

typedef struct {
    mini_value_kind kind;
    union {
        int64_t integer;
        bool boolean;
        mini_range range;
    } as;
} mini_value;

bool mini_run_program(const node *program, mdr_buffer *out, mdr_mini_error *error);
bool mini_emit_c_program(const node *program, mdr_buffer *out, mdr_mini_error *error);

const char *mini_value_kind_name(mini_value_kind kind);

#endif