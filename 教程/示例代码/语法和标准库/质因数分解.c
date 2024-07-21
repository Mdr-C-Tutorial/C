#include <stdio.h>
int main(void)
{
    unsigned long long n, i;
    scanf("%llu", &n);
    for (i = 2; n /= 2; ++i)
    {
        for (; n % i == 0; n /= i)
        {
            printf("%llu\n", i);
        }
    }
    return 0;
}