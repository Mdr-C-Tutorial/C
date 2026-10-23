#include <stdio.h>

/* 这一行注释在预处理后消失 */
#define GREETING "Hello, C!"

int main(void) {
    printf(GREETING "\n");
    return 0;
}
