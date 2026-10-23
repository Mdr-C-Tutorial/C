/* 22116 的起始文件：从旧项目抄来，在 MinGW 上能构建，在 glibc 上未必。 */

#define strlcpy(dst, src, siz) strlcpy_debug(dst, src, siz, __FILE__, __LINE__)

#include <stddef.h>
#include <stdio.h>
#include <string.h>

int _count = 0;
int __debug_level = 0;

size_t strlcpy_debug(char *dst, const char *src, size_t siz, const char *file,
                     int line);

size_t strlcpy_debug(char *dst, const char *src, size_t siz, const char *file,
                     int line) {
    size_t n = strlen(src);
    size_t k;

    (void)file;
    (void)line;
    if (siz > 0) {
        k = n < siz - 1 ? n : siz - 1;
        memcpy(dst, src, k);
        dst[k] = '\0';
    }
    return n;
}

int main(void) {
    char buf[8];
    size_t n;

    _count++;
    __debug_level = 1;
    n = strlcpy(buf, "hello", sizeof buf);
    printf("%d %d %d %s\n", (int)n, _count, __debug_level, buf);
    return 0;
}
