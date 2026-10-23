#include <stdio.h>
#include <stdlib.h>

int main(void) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) != -1) {
        printf("%zd 字节：%s", len, line);
    }
    if (ferror(stdin)) {
        perror("getline");
        return 1;
    }
    free(line);
    printf("stdin 的文件描述符是 %d\n", fileno(stdin));
    return 0;
}
