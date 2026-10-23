#include "cli.h"

#include <stdio.h>
#include <string.h>

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

int cli_split(char *line, char **argv, int max) {
    int argc = 0;
    char *p = line;

    while (*p != '\0' && argc < max) {
        while (*p != '\0' && is_space(*p)) {
            *p++ = '\0';
        }
        if (*p == '\0') {
            break;
        }
        argv[argc++] = p;
        while (*p != '\0' && !is_space(*p)) {
            ++p;
        }
    }
    return argc;
}

int cli_dispatch(const struct cli_command *table, size_t count, char *line) {
    char *p = line;

    while (*p != '\0' && is_space(*p)) {
        ++p;
    }
    char *name = p;
    while (*p != '\0' && !is_space(*p)) {
        ++p;
    }
    if (*p != '\0') {
        *p++ = '\0';
    }
    if (*name == '\0') {
        return 0;
    }
    while (*p != '\0' && is_space(*p)) {
        ++p;
    }
    /* 去掉参数串末尾的空白，命令函数拿到的就是干净的一段 */
    size_t len = strlen(p);
    while (len > 0 && is_space(p[len - 1])) {
        p[--len] = '\0';
    }

    for (size_t i = 0; i < count; ++i) {
        if (strcmp(name, table[i].name) == 0) {
            return table[i].run(p);
        }
    }
    printf("未知命令: %s（输入 help 查看可用命令）\n", name);
    return -1;
}

void cli_print_help(const struct cli_command *table, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        printf("  %-8s %s\n", table[i].name, table[i].help);
    }
}