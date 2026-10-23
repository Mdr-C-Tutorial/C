/*
 * 测试用的读文件实现：不打开任何文件，固定返回下面这段文本。
 * 与 reader.c 提供同一个 reader_load，链接时二选一。
 */
#include <stdlib.h>
#include <string.h>

#include "reader.h"

static const char kText[] = "the cat sat\non the mat\n";

char *reader_load(const char *path, size_t *len) {
    (void)path;
    size_t n = sizeof kText - 1;
    char *copy = malloc(n + 1);
    if (copy != NULL) {
        memcpy(copy, kText, n + 1);
        *len = n;
    }
    return copy;
}
