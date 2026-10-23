#ifndef CLI_H
#define CLI_H

#include <stddef.h>

/*
 * 极简命令行：一张命令表 + 按空白分词。
 *
 * 命令函数收到的是参数串（不含命令名），可以整段交给 strtol/strcmp；
 * 需要多个参数的命令自己再分词。返回 0 表示成功，非 0 会打印用法。
 */
struct cli_command {
    const char *name;
    const char *help;
    int (*run)(const char *args);
};

/* 把 line 原地切成以空白分隔的若干段，argv 最多放 max 个；返回段数。 */
int cli_split(char *line, char **argv, int max);

/* 查找并执行命令；未知命令打印提示。返回命令函数的返回值，未知命令返回 -1。 */
int cli_dispatch(const struct cli_command *table, size_t count, char *line);

/* 打印命令表。 */
void cli_print_help(const struct cli_command *table, size_t count);

#endif /* CLI_H */