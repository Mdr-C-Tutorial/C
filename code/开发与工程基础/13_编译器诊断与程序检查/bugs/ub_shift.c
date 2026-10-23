/*
 * 第 13 章 UBSan 示例二：移位位数超出类型的宽度。
 *   gcc -std=c17 -Wall -Wextra -g -fsanitize=undefined -o ub_shift bugs/ub_shift.c
 */
#include <stdio.h>

int main(void) {
    unsigned int shift = 40;
    unsigned int value = 1u << shift;
    printf("%u\n", value);
    return 0;
}
