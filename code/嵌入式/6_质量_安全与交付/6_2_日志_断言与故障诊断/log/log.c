/*
 * log.c：日志的格式化与输出。
 *
 * 一行日志的格式是 `[  1234] I tag: 正文`。缓冲放在栈上，
 * 不用 malloc；格式化用 vsnprintf，不用 printf。
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <stdarg.h>
#include <stdio.h>

#include "log.h"

#define LOG_LINE_MAX 128u

void log_emit(char level_char, const char *tag, const char *fmt, ...) {
    char line[LOG_LINE_MAX];
    int prefix;
    int used;
    va_list args;

    prefix = snprintf(line, sizeof line, "[%6lu] %c %s: ",
                      (unsigned long)log_tick_ms(), level_char, tag);
    if (prefix < 0 || (size_t)prefix >= sizeof line) {
        return;
    }

    va_start(args, fmt);
    used = vsnprintf(line + prefix, sizeof line - (size_t)prefix, fmt, args);
    va_end(args);
    if (used < 0) {
        return;
    }

    size_t total = (size_t)prefix + (size_t)used;
    if (total > sizeof line - 2u) {
        total = sizeof line - 2u;
    }
    line[total] = '\n';
    line[total + 1u] = '\0';
    log_write(line, total + 1u);
}