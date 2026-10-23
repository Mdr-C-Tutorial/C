#include "check.h"
#include <stdarg.h>
#include <stdio.h>

static size_t total_checks = 0;
static size_t failed_checks = 0;

void check_pass(void) {
    total_checks++;
}

void check_fail(const char *file, int line, const char *fmt, ...) {
    va_list ap;

    total_checks++;
    failed_checks++;
    fprintf(stderr, "%s:%d: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

int check_report(const char *suite) {
    printf("%s: %zu checks, %zu failed\n", suite, total_checks, failed_checks);
    return failed_checks == 0 ? 0 : 1;
}
