/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <stdio.h>

void pyramid(char c)
{
    int height = c - 'A' + 1;
    int width = height * 2 - 1;
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < height - i - 1; ++j) {
            putchar(' ');
        }
        for (int j = 0; j < i + 1; ++j) {
            putchar('A' + j);
        }
        for (int j = i - 1; j >= 0; --j) {
            putchar('A' + j);
        }
        putchar('\n');
    }
}

int main()
{
    char c;
    scanf("%c", &c);
    pyramid(c);
    return 0;
}