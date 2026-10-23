/* upper.c：把标准输入逐字节转换成大写后写到标准输出 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int c;
    while ((c = getchar()) != EOF) {
        if (putchar(toupper((unsigned char)c)) == EOF) {
            perror("upper");
            return EXIT_FAILURE;
        }
    }
    if (ferror(stdin)) {
        perror("upper");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
