#include <stdio.h>

void bubble_sort(int *p, int len)
{
    for (int i = 0; i < len; ++i)
    {
        for (int j = 0; j < i; j++)
        {
            if (p[i] < p[j])
            {
                int temp = p[i];
                p[i] = p[j];
                p[j] = temp;
            }
        }
    }
}

int main()
{
    int arr[10] = {432, 346, 24, 46, 124, 47, 57734, 7432, 62, 632};
    bubble_sort(arr, 10);
    for (int i = 0; i < 10; ++i)
    {
        printf("arr[%d]=%d\n", i, arr[i]);
    }
    return 0;
}