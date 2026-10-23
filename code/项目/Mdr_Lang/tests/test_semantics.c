#define _POSIX_C_SOURCE 200809L

#include "../../compiler_c/include/mdr_compiler.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CASES 128

typedef struct {
    char *name;
    char *source;
} test_case;

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
    if (end < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    size_t size = (size_t)end;
    char *text = (char *)malloc(size + 1u);
    if (text == NULL) {
        fclose(fp);
        return NULL;
    }
    if (fread(text, 1u, size, fp) != size) {
        free(text);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    text[size] = '\0';
    return text;
}

static int compare_case(const void *left, const void *right) {
    const test_case *a = (const test_case *)left;
    const test_case *b = (const test_case *)right;
    return strcmp(a->name, b->name);
}

static size_t load_cases(const char *dir_path, test_case *cases, size_t max_cases) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        fprintf(stderr, "cannot open directory: %s\n", dir_path);
        return 0u;
    }
    size_t count = 0u;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_cases) {
        size_t name_len = strlen(entry->d_name);
        if (name_len < 4u || strcmp(entry->d_name + name_len - 4u, ".mdr") != 0) {
            continue;
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        char *source = read_all_text(path);
        if (source == NULL) {
            fprintf(stderr, "cannot read: %s\n", path);
            continue;
        }
        cases[count].name = strdup(entry->d_name);
        cases[count].source = source;
        count += 1u;
    }
    closedir(dir);
    qsort(cases, count, sizeof(cases[0]), compare_case);
    return count;
}

static const char *find_expect_error(const char *source, size_t *out_line, size_t *out_column,
                                     char *out_message, size_t message_size) {
    const char *marker = strstr(source, "// expect-error:");
    if (marker == NULL) {
        return NULL;
    }
    marker += strlen("// expect-error:");
    while (*marker == ' ' || *marker == '\t') {
        marker += 1;
    }
    unsigned long line = 0u;
    unsigned long column = 0u;
    int consumed = 0;
    if (sscanf(marker, "%lu:%lu:%n", &line, &column, &consumed) != 2) {
        return NULL;
    }
    const char *message = marker + consumed;
    while (*message == ' ') {
        message += 1;
    }
    size_t len = strcspn(message, "\r\n");
    if (len >= message_size) {
        len = message_size - 1u;
    }
    memcpy(out_message, message, len);
    out_message[len] = '\0';
    *out_line = (size_t)line;
    *out_column = (size_t)column;
    return message;
}

static bool run_pass_cases(const char *root) {
    char dir_path[1024];
    snprintf(dir_path, sizeof(dir_path), "%s/pass", root);
    test_case cases[MAX_CASES] = {0};
    size_t count = load_cases(dir_path, cases, MAX_CASES);
    if (count == 0u) {
        printf("FAIL pass: no cases in %s\n", dir_path);
        return false;
    }
    bool all_ok = true;
    for (size_t i = 0; i < count; ++i) {
        mdr_error error = {0};
        bool ok = mdr_compile_text(cases[i].name, cases[i].source, false, &error);
        if (ok) {
            printf("PASS pass/%s\n", cases[i].name);
        } else {
            printf("FAIL pass/%s: expected OK, got %zu:%zu: %s\n", cases[i].name, error.line,
                   error.column, error.message);
            all_ok = false;
        }
        free(cases[i].name);
        free(cases[i].source);
    }
    printf("pass: %zu cases\n", count);
    return all_ok;
}

static bool run_fail_cases(const char *root) {
    char dir_path[1024];
    snprintf(dir_path, sizeof(dir_path), "%s/fail", root);
    test_case cases[MAX_CASES] = {0};
    size_t count = load_cases(dir_path, cases, MAX_CASES);
    if (count == 0u) {
        printf("FAIL fail: no cases in %s\n", dir_path);
        return false;
    }
    bool all_ok = true;
    for (size_t i = 0; i < count; ++i) {
        size_t line = 0u;
        size_t column = 0u;
        char expected[256] = {0};
        if (find_expect_error(cases[i].source, &line, &column, expected, sizeof(expected)) == NULL) {
            printf("FAIL fail/%s: missing // expect-error line\n", cases[i].name);
            all_ok = false;
            continue;
        }
        mdr_error error = {0};
        bool ok = mdr_compile_text(cases[i].name, cases[i].source, false, &error);
        if (ok) {
            printf("FAIL fail/%s: expected %zu:%zu: %s, got OK\n", cases[i].name, line, column,
                   expected);
            all_ok = false;
        } else if (error.line != line || error.column != column ||
                   strcmp(error.message, expected) != 0) {
            printf("FAIL fail/%s: expected %zu:%zu: %s, got %zu:%zu: %s\n", cases[i].name, line,
                   column, expected, error.line, error.column, error.message);
            all_ok = false;
        } else {
            printf("PASS fail/%s\n", cases[i].name);
        }
        free(cases[i].name);
        free(cases[i].source);
    }
    printf("fail: %zu cases\n", count);
    return all_ok;
}

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : "cases";
    bool ok = run_pass_cases(root);
    ok = run_fail_cases(root) && ok;
    return ok ? 0 : 1;
}