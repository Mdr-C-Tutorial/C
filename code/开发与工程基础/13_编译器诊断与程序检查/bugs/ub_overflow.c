/*
 * 第 13 章 UBSan 示例一：有符号整数溢出。
 *   gcc -std=c17 -Wall -Wextra -g -fsanitize=undefined -o ub_overflow bugs/ub_overflow.c
 * 实参来自 argc，运行时才知道结果，编译器不能把整个表达式折叠掉。
 */
#include <limits.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int value = INT_MAX;
    value += argc;
    printf("%d %s\n", value, argv[0]);
    return 0;
}
