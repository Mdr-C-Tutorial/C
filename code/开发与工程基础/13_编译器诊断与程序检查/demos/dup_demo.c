#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 第 13 章缺陷 (a) 的最小复现：只分配 strlen(s) 个字节 */
static char *dup_string(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

int main(void) {
    char *word = dup_string("the");
    printf("%s\n", word);
    free(word);
    return 0;
}
