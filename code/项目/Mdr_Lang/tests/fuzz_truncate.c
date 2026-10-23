#define _POSIX_C_SOURCE 200809L

#include "../../compiler_c/include/mdr_compiler.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CASES 128

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

static int compare_names(const void *left, const void *right) {
    const char *const *a = (const char *const *)left;
    const char *const *b = (const char *const *)right;
    return strcmp(*a, *b);
}

/* 把一个源文本的每个字节边界都当成截断点，逐次送给编译器。
 * 截断点落在多字节 UTF-8 序列中间时，输入含无效字节序列，
 * 编译器把它们当普通字符处理，不会越界。 */
static size_t truncate_source(const char *name, const char *source) {
    size_t length = strlen(source);
    size_t count = 0u;
    for (size_t cut = 0u; cut <= length; ++cut) {
        char *partial = (char *)malloc(cut + 1u);
        if (partial == NULL) {
            return count;
        }
        memcpy(partial, source, cut);
        partial[cut] = '\0';
        mdr_error error = {0};
        (void)mdr_compile_text(name, partial, false, &error);
        free(partial);
        count += 1u;
    }
    return count;
}

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : "cases";
    const char *subdirs[] = {"pass", "fail", "run"};
    size_t total = 0u;
    for (size_t d = 0; d < sizeof(subdirs) / sizeof(subdirs[0]); ++d) {
        char dir_path[1024];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", root, subdirs[d]);
        DIR *dir = opendir(dir_path);
        if (dir == NULL) {
            fprintf(stderr, "cannot open directory: %s\n", dir_path);
            return 1;
        }
        char **names = NULL;
        size_t count = 0u;
        size_t cap = 0u;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            size_t name_len = strlen(entry->d_name);
            if (name_len < 4u || strcmp(entry->d_name + name_len - 4u, ".mdr") != 0) {
                continue;
            }
            if (count == cap) {
                size_t next = cap == 0u ? 16u : cap * 2u;
                char **grown = (char **)realloc(names, next * sizeof(*grown));
                if (grown == NULL) {
                    free(names);
                    closedir(dir);
                    return 1;
                }
                names = grown;
                cap = next;
            }
            names[count] = strdup(entry->d_name);
            count += 1u;
        }
        closedir(dir);
        qsort(names, count, sizeof(names[0]), compare_names);
        for (size_t i = 0; i < count; ++i) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir_path, names[i]);
            char *source = read_all_text(path);
            if (source == NULL) {
                fprintf(stderr, "cannot read: %s\n", path);
                free(names[i]);
                continue;
            }
            size_t runs = truncate_source(names[i], source);
            printf("PASS truncate %s/%s: %zu prefixes\n", subdirs[d], names[i], runs);
            total += runs;
            free(source);
            free(names[i]);
        }
        free(names);
    }
    printf("truncate: %zu inputs\n", total);
    return 0;
}