#include "../include/mdr_mini.h"

#include "mini_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all_text(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long end = ftell(fp);
    if (end < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    size_t size = (size_t)end;
    char *text = (char *)malloc(size + 1u);
    if (text == NULL) {
        fclose(fp);
        return NULL;
    }
    size_t read = fread(text, 1u, size, fp);
    fclose(fp);
    if (read != size) {
        free(text);
        return NULL;
    }
    text[size] = '\0';
    return text;
}

static void print_usage(void) {
    puts("Usage: mdr_mini <input.mdr> [--emit-c]");
}

int main(int argc, char **argv) {
    const char *input = NULL;
    bool emit_c = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--emit-c") == 0) {
            emit_c = true;
            continue;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 65;
        }
        if (input != NULL) {
            fputs("Only one input file is supported.\n", stderr);
            return 65;
        }
        input = argv[i];
    }

    if (input == NULL) {
        print_usage();
        return 65;
    }

    char *source = read_all_text(input);
    if (source == NULL) {
        fprintf(stderr, "Failed to read input file: %s\n", input);
        return 65;
    }

    mdr_buffer out = {0};
    mdr_mini_error error = {0};
    bool ok = emit_c ? mdr_mini_emit_c(source, &out, &error) : mdr_mini_run(source, &out, &error);
    free(source);

    if (!ok) {
        fprintf(stderr, "%s:%zu:%zu: %s\n", input, error.line, error.column, error.message);
        mdr_buffer_free(&out);
        return error.runtime ? 70 : 65;
    }

    if (out.len > 0u) {
        fwrite(out.data, 1u, out.len, stdout);
    }
    mdr_buffer_free(&out);
    return 0;
}