#include "mini_internal.h"

#include <ctype.h>
#include <string.h>

static const char *const k_mini_keywords[] = {
    "let", "const", "if",   "else",  "while", "loop", "for",
    "break", "continue", "print", "true", "false",
};

static const char *const k_mini_operators[] = {
    "...", "..", "||", "&&", "==", "!=", ">=", "<=", "+=", "-=", "*=", "/=",
    "=",   ">",  "<",  "+",  "-",  "*",  "/",  "!",  "(",  ")",  "{",  "}",
    ",",   ":",  ";",  ".",  "->",
};

bool mdr_buffer_append(mdr_buffer *buffer, const char *text, size_t len) {
    if (buffer->len + len + 1u > buffer->cap) {
        size_t next = buffer->cap == 0u ? 256u : buffer->cap;
        while (next < buffer->len + len + 1u) {
            next *= 2u;
        }
        char *data = (char *)realloc(buffer->data, next);
        if (data == NULL) {
            return false;
        }
        buffer->data = data;
        buffer->cap = next;
    }
    memcpy(buffer->data + buffer->len, text, len);
    buffer->len += len;
    buffer->data[buffer->len] = '\0';
    return true;
}

bool mdr_buffer_append_str(mdr_buffer *buffer, const char *text) {
    return mdr_buffer_append(buffer, text, strlen(text));
}

bool mdr_buffer_appendf(mdr_buffer *buffer, const char *format, ...) {
    char stack_buf[512];
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(stack_buf, sizeof(stack_buf), format, args);
    va_end(args);
    if (needed < 0) {
        return false;
    }
    if ((size_t)needed < sizeof(stack_buf)) {
        return mdr_buffer_append(buffer, stack_buf, (size_t)needed);
    }
    char *heap_buf = (char *)malloc((size_t)needed + 1u);
    if (heap_buf == NULL) {
        return false;
    }
    va_start(args, format);
    vsnprintf(heap_buf, (size_t)needed + 1u, format, args);
    va_end(args);
    bool ok = mdr_buffer_append(buffer, heap_buf, (size_t)needed);
    free(heap_buf);
    return ok;
}

void mdr_buffer_free(mdr_buffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->len = 0u;
    buffer->cap = 0u;
}

void mini_set_error(mdr_mini_error *error,
                    bool runtime,
                    size_t line,
                    size_t column,
                    const char *format,
                    ...) {
    if (error == NULL) {
        return;
    }
    error->runtime = runtime;
    error->line = line;
    error->column = column;
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

static bool text_equals(const char *text, size_t len, const char *word) {
    return strlen(word) == len && strncmp(text, word, len) == 0;
}

static bool token_push(mini_token **items, size_t *len, size_t *cap, mini_token item) {
    if (*len == *cap) {
        size_t next = *cap == 0u ? 64u : *cap * 2u;
        mini_token *mem = (mini_token *)realloc(*items, next * sizeof(*mem));
        if (mem == NULL) {
            return false;
        }
        *items = mem;
        *cap = next;
    }
    (*items)[(*len)++] = item;
    return true;
}

bool mini_lex(const char *source, mini_token **out_tokens, size_t *out_count, mdr_mini_error *error) {
    mini_token *items = NULL;
    size_t len = 0u;
    size_t cap = 0u;
    size_t offset = 0u;
    size_t line = 1u;
    size_t column = 1u;
    size_t length = strlen(source);

    while (offset < length) {
        char c = source[offset];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') {
                line += 1u;
                column = 1u;
            } else {
                column += 1u;
            }
            offset += 1u;
            continue;
        }
        if (c == '/' && offset + 1u < length && source[offset + 1u] == '/') {
            offset += 2u;
            column += 2u;
            while (offset < length && source[offset] != '\n') {
                offset += 1u;
                column += 1u;
            }
            continue;
        }

        size_t start = offset;
        size_t start_line = line;
        size_t start_column = column;
        mini_token tok = {0};

        if (isdigit((unsigned char)c)) {
            int64_t value = 0;
            while (offset < length && isdigit((unsigned char)source[offset])) {
                value = value * 10 + (source[offset] - '0');
                offset += 1u;
                column += 1u;
            }
            tok.kind = MINI_TOKEN_INT;
            tok.int_value = value;
        } else if (isalpha((unsigned char)c) || c == '_') {
            offset += 1u;
            column += 1u;
            while (offset < length) {
                char ch = source[offset];
                if (!isalnum((unsigned char)ch) && ch != '_') {
                    break;
                }
                offset += 1u;
                column += 1u;
            }
            size_t text_len = offset - start;
            tok.kind = MINI_TOKEN_IDENTIFIER;
            for (size_t i = 0; i < sizeof(k_mini_keywords) / sizeof(k_mini_keywords[0]); ++i) {
                if (text_equals(source + start, text_len, k_mini_keywords[i])) {
                    tok.kind = MINI_TOKEN_KEYWORD;
                    break;
                }
            }
        } else {
            size_t matched = 0u;
            for (size_t i = 0; i < sizeof(k_mini_operators) / sizeof(k_mini_operators[0]); ++i) {
                size_t op_len = strlen(k_mini_operators[i]);
                if (offset + op_len <= length &&
                    strncmp(source + offset, k_mini_operators[i], op_len) == 0) {
                    matched = op_len;
                    break;
                }
            }
            if (matched == 0u) {
                free(items);
                mini_set_error(error, false, start_line, start_column, "unexpected character '%c'", c);
                return false;
            }
            offset += matched;
            column += matched;
            tok.kind = MINI_TOKEN_OPERATOR;
        }

        tok.begin = source + start;
        tok.len = offset - start;
        tok.line = start_line;
        tok.column = start_column;
        tok.end_line = line;
        tok.end_column = column;
        if (!token_push(&items, &len, &cap, tok)) {
            free(items);
            mini_set_error(error, false, start_line, start_column, "out of memory");
            return false;
        }
    }

    mini_token eof = {.kind = MINI_TOKEN_EOF,
                      .begin = source + length,
                      .len = 0u,
                      .line = line,
                      .column = column,
                      .end_line = line,
                      .end_column = column};
    if (!token_push(&items, &len, &cap, eof)) {
        free(items);
        mini_set_error(error, false, line, column, "out of memory");
        return false;
    }

    *out_tokens = items;
    *out_count = len;
    return true;
}

void mini_lex_free(mini_token *tokens) {
    free(tokens);
}