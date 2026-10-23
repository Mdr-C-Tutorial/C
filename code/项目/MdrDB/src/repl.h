/*
 * repl.h：命令行界面（read-eval-print loop）。
 *
 * 每行一条命令，按空白切成单词，第一个词决定做什么。普通命令读写表里
 * 的数据；以 '.' 开头的元命令（meta command）用来观察数据库内部。
 */
#ifndef MDRDB_REPL_H
#define MDRDB_REPL_H

#include "mdrdb.h"

#include <stdbool.h>
#include <stdio.h>

#define REPL_LINE_MAX 1024

typedef enum repl_result { REPL_CONTINUE, REPL_EXIT } repl_result;

/* 执行一行命令，结果写到 out。line 会被改写（切词时写入 '\0'）。 */
repl_result repl_execute(db *d, char *line, FILE *out);

/*
 * 从 in 逐行读命令并执行，直到 .exit 或输入结束。echo 为 true 时把
 * 读到的命令原样打印在提示符后面，管道输入的输出因此和交互时一样。
 */
void repl_run(db *d, FILE *in, FILE *out, bool echo);

#endif
