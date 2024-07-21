#include <stdio.h>

int manhattan_distance(int x, int y, int x0, int y0)
{
    int dx, dy;
    if (x < x0)
    {
        dx = x0 - x;
    }
    else
    {
        dx = x - x0;
    }
    if (y < y0)
    {
        dy = y0 - y;
    }
    else
    {
        dy = y - y0;
    }
    return dx + dy;
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
            {
                putchar(c - distance);
            }
            else
            {
                putchar(' ');
            }
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