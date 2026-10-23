#include "check.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 计数只在本文件里，头文件不定义对象 */
static int total_checks = 0;
static int failed_checks = 0;

void check_pass(void) { total_checks++; }

void check_fail(const char *file, int line, const char *fmt, ...) {
    total_checks++;
    failed_checks++;

    fprintf(stderr, "%s:%d: ", file, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

int check_report(const char *suite) {
    printf("%s: %d checks, %d failed\n", suite, total_checks, failed_checks);
    return failed_checks == 0 ? 0 : 1;
}
