#include <stdio.h>

int manhattan_distance(int x, int y, int x0, int y0)
{
    return ((x - x0) >= 0 ? (x - x0) : (x0 - x)) + ((y - y0) >= 0 ? (y - y0) : (y0 - y));
}

void pyramid(char c)
{
    int height = c - 'A' + 1;
    int width = height * 2 - 1;
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            int distance = manhattan_distance(i, j, height - 1, height - 1);
            if (c - distance >= 'A')
                putchar(c - distance);
            else
                putchar(' ');
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