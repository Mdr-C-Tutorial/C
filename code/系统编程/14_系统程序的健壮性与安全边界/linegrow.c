#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* 从标准输入读一行，报告缓冲区最大容量。
 * 不带参数时不限长度；带参数时超过给定字节数就放弃这一行。 */
int main(int argc, char *argv[]) {
    size_t limit = 0;
    if (argc > 1) {
        limit = strtoul(argv[1], NULL, 10);
    }
    size_t cap = 4096;
    size_t len = 0;
    char *line = malloc(cap);
    if (line == NULL) {
        perror("malloc");
        return 1;
    }
    for (;;) {
        if (len + 1 == cap) {
            cap *= 2;
            line = realloc(line, cap);
            if (line == NULL) {
                perror("realloc");
                return 1;
            }
        }
        ssize_t n = read(STDIN_FILENO, line + len, cap - len - 1);
        if (n == -1) {
            perror("read");
            free(line);
            return 1;
        }
        if (n == 0) {
            break;
        }
        len += (size_t)n;
        if (limit != 0 && len > limit) {
            printf("line too long: %zu bytes read, limit %zu, buffer %zu\n", len,
                   limit, cap);
            free(line);
            return 1;
        }
        if (line[len - 1] == '\n') {
            break;
        }
    }
    printf("line read: %zu bytes, buffer capacity %zu\n", len, cap);
    free(line);
    return 0;
}