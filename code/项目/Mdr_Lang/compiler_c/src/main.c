#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../include/mdr_compiler.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all_text(const char *file_path) {
    FILE *fp = fopen(file_path, "rb");
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

    size_t nread = fread(text, 1u, size, fp);
    fclose(fp);
    if (nread != size) {
        free(text);
        return NULL;
    }

    text[size] = '\0';
    return text;
}

static void print_usage(void) {
    puts("Usage: mdrcc <input.mdr> [--tokens]");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *input = NULL;
    bool dump_tokens = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--tokens") == 0) {
            dump_tokens = true;
            continue;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
        if (input != NULL) {
            fputs("Only one input file is supported.\n", stderr);
            return 1;
        }
        input = argv[i];
    }

    if (input == NULL) {
        print_usage();
        return 1;
    }

    char *text = read_all_text(input);
    if (text == NULL) {
        fprintf(stderr, "Failed to read input file: %s\n", input);
        return 1;
    }

    mdr_error error = {0};
    bool ok = mdr_compile_text(input, text, dump_tokens, &error);
    free(text);

    if (!ok) {
        fprintf(stderr, "%s:%zu:%zu: %s\n", error.file_path, error.line, error.column, error.message);
        return 1;
    }

    printf("OK: %s\n", input);
    return 0;
}
