#ifndef MDR_MINI_H
#define MDR_MINI_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} mdr_buffer;

typedef struct {
    bool runtime;
    size_t line;
    size_t column;
    char message[256];
} mdr_mini_error;

bool mdr_mini_run(const char *source, mdr_buffer *out, mdr_mini_error *error);
bool mdr_mini_emit_c(const char *source, mdr_buffer *out, mdr_mini_error *error);
void mdr_buffer_free(mdr_buffer *buffer);

#endif