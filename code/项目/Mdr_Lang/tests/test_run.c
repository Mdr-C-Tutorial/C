#define _POSIX_C_SOURCE 200809L

#include "../../mini/include/mdr_mini.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define change_dir _chdir
#define make_dir(path) _mkdir(path)
#else
#include <unistd.h>
#define change_dir chdir
#define make_dir(path) mkdir(path, 0777)
#endif

#define MAX_CASES 64

typedef struct {
    char *name;
    char *source;
    char *expected;
} run_case;

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

static void strip_carriage_returns(char *text) {
    char *write = text;
    for (char *read = text; *read != '\0'; ++read) {
        if (*read != '\r') {
            *write++ = *read;
        }
    }
    *write = '\0';
}

static int compare_case(const void *left, const void *right) {
    const run_case *a = (const run_case *)left;
    const run_case *b = (const run_case *)right;
    return strcmp(a->name, b->name);
}

static char *extract_expected(const char *source) {
    static const char marker[] = "// expect:";
    size_t capacity = 256u;
    size_t len = 0u;
    char *text = (char *)malloc(capacity);
    if (text == NULL) {
        return NULL;
    }
    text[0] = '\0';
    const char *cursor = source;
    while ((cursor = strstr(cursor, marker)) != NULL) {
        cursor += sizeof(marker) - 1u;
        while (*cursor == ' ') {
            cursor += 1;
        }
        size_t line_len = strcspn(cursor, "\r\n");
        size_t needed = len + line_len + 2u;
        if (needed > capacity) {
            while (capacity < needed) {
                capacity *= 2u;
            }
            char *grown = (char *)realloc(text, capacity);
            if (grown == NULL) {
                free(text);
                return NULL;
            }
            text = grown;
        }
        if (len > 0u) {
            text[len++] = '\n';
        }
        memcpy(text + len, cursor, line_len);
        len += line_len;
        text[len] = '\0';
    }
    return text;
}

static size_t load_cases(const char *dir_path, run_case *cases, size_t max_cases) {
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
        cases[count].expected = extract_expected(source);
        count += 1u;
    }
    closedir(dir);
    qsort(cases, count, sizeof(cases[0]), compare_case);
    return count;
}

static bool compare_output(const char *label, const char *expected, const char *actual) {
    size_t expected_len = strlen(expected);
    while (expected_len > 0u && expected[expected_len - 1u] == '\n') {
        expected_len -= 1u;
    }
    size_t actual_len = strlen(actual);
    while (actual_len > 0u && actual[actual_len - 1u] == '\n') {
        actual_len -= 1u;
    }
    if (expected_len == actual_len && strncmp(expected, actual, expected_len) == 0) {
        return true;
    }
    printf("FAIL %s\n--- expected\n%s\n--- got\n%s\n--- end\n", label, expected, actual);
    return false;
}

static bool write_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return false;
    }
    bool ok = fwrite(text, 1u, strlen(text), fp) == strlen(text);
    fclose(fp);
    return ok;
}

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : "cases";
    const char *work_dir = argc > 2 ? argv[2] : "mdr_test_run_tmp";
    char dir_path[1024];
    snprintf(dir_path, sizeof(dir_path), "%s/run", root);

    run_case cases[MAX_CASES] = {0};
    size_t count = load_cases(dir_path, cases, MAX_CASES);
    if (count == 0u) {
        printf("FAIL run: no cases in %s\n", dir_path);
        return 1;
    }

    make_dir(work_dir);
    if (change_dir(work_dir) != 0) {
        printf("FAIL run: cannot enter work directory %s\n", work_dir);
        return 1;
    }

    bool all_ok = true;
    for (size_t i = 0; i < count; ++i) {
        char label[256];
        snprintf(label, sizeof(label), "run/%s", cases[i].name);

        mdr_buffer interpreted = {0};
        mdr_mini_error error = {0};
        if (!mdr_mini_run(cases[i].source, &interpreted, &error)) {
            printf("FAIL %s: interpreter error at %zu:%zu: %s\n", label, error.line, error.column,
                   error.message);
            all_ok = false;
            goto next;
        }
        if (!compare_output(label, cases[i].expected,
                            interpreted.data == NULL ? "" : interpreted.data)) {
            all_ok = false;
            goto next;
        }

        mdr_buffer generated = {0};
        if (!mdr_mini_emit_c(cases[i].source, &generated, &error)) {
            printf("FAIL %s: emit-c error at %zu:%zu: %s\n", label, error.line, error.column,
                   error.message);
            all_ok = false;
            goto next;
        }

        char stem[128];
        snprintf(stem, sizeof(stem), "%s", cases[i].name);
        stem[strlen(stem) - 4u] = '\0';

        char c_path[256];
        snprintf(c_path, sizeof(c_path), "%s.c", stem);
        if (!write_file(c_path, generated.data == NULL ? "" : generated.data)) {
            printf("FAIL %s: cannot write %s\n", label, c_path);
            all_ok = false;
            goto next;
        }

        char compile_cmd[1024];
        snprintf(compile_cmd, sizeof(compile_cmd),
                 "gcc -std=c11 -Wall -Wextra -Werror -o %s.exe %s.c", stem, stem);
        if (system(compile_cmd) != 0) {
            printf("FAIL %s: generated C does not compile\n", label);
            all_ok = false;
            goto next;
        }

        char run_cmd[1024];
#ifdef _WIN32
        snprintf(run_cmd, sizeof(run_cmd), ".\\%s.exe > .\\%s.out", stem, stem);
#else
        snprintf(run_cmd, sizeof(run_cmd), "./%s.exe > ./%s.out", stem, stem);
#endif
        if (system(run_cmd) != 0) {
            printf("FAIL %s: generated program failed to run\n", label);
            all_ok = false;
            goto next;
        }

        char out_path[256];
        snprintf(out_path, sizeof(out_path), "%s.out", stem);
        char *compiled_output = read_all_text(out_path);
        if (compiled_output == NULL) {
            printf("FAIL %s: cannot read %s\n", label, out_path);
            all_ok = false;
            goto next;
        }
        strip_carriage_returns(compiled_output);
        if (!compare_output(label, cases[i].expected, compiled_output)) {
            printf("    (above: generated C differs from the interpreter)\n");
            all_ok = false;
            free(compiled_output);
            goto next;
        }
        free(compiled_output);
        printf("PASS %s\n", label);

    next:
        free(cases[i].name);
        free(cases[i].source);
        free(cases[i].expected);
    }

    printf("run: %zu cases\n", count);
    return all_ok ? 0 : 1;
}