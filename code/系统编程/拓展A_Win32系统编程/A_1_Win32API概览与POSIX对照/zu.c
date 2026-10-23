/* zu.c：size_t 与 %zu，看 MinGW 的格式检查怎么报。 */
#include <stdio.h>

int main(void) {
    size_t n = 42;
    printf("%zu\n", n);
    return 0;
}