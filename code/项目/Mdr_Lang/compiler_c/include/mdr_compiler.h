#ifndef MDR_COMPILER_H
#define MDR_COMPILER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *file_path;
    size_t line;
    size_t column;
    char message[256];
} mdr_error;

bool mdr_compile_text(const char *file_path, const char *source, bool dump_tokens, mdr_error *error);

#endif
