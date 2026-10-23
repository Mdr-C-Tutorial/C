#include "../include/mdr_mini.h"

#include "mini_internal.h"

#include <stdlib.h>
#include <string.h>

bool mdr_mini_run(const char *source, mdr_buffer *out, mdr_mini_error *error) {
    arena a;
    arena_init(&a);
    node *program = NULL;
    bool ok = mini_parse(source, &a, &program, error);
    if (ok) {
        ok = mini_run_program(program, out, error);
    }
    arena_free(&a);
    return ok;
}

bool mdr_mini_emit_c(const char *source, mdr_buffer *out, mdr_mini_error *error) {
    arena a;
    arena_init(&a);
    node *program = NULL;
    bool ok = mini_parse(source, &a, &program, error);
    if (ok) {
        ok = mini_emit_c_program(program, out, error);
    }
    arena_free(&a);
    return ok;
}