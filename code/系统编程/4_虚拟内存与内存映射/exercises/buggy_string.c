#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 这份代码里有一处差一错误：分配的空间刚好放得下字符串本身，
 * 没有给结尾的 '\0' 留位置。用普通 malloc 时它常常看不出问题。 */
static char *duplicate_trimmed(const char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == ' ') {
        n--;
    }
    char *p = malloc(n);
    if (p == NULL) {
        return NULL;
    }
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

int main(void) {
    const char *samples[] = {"alpha", "beta   ", "gamma"};
    setvbuf(stdout, NULL, _IONBF, 0);
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; i++) {
        printf("处理 [%s]……\n", samples[i]);
        char *copy = duplicate_trimmed(samples[i]);
        if (copy == NULL) {
            return EXIT_FAILURE;
        }
        printf("得到 [%s]\n", copy);
        free(copy);
    }
    return 0;
}
