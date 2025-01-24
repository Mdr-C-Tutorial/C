/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <stdio.h>

int main(void) {
    int a = 1;
    int b = 1;

    printf("a = %d, b = %d\n", a, b);     // a = 1, b = 1
    printf("a = %d, b = %d\n", a++, ++b); // a = 1, b = 2
    printf("a = %d, b = %d\n", a, b);     // a = 2, b = 2
    printf("a = %d, b = %d\n", a--, --b); // a = 2, b = 1
    printf("a = %d, b = %d\n", a, b);     // a = 1, b = 1
}