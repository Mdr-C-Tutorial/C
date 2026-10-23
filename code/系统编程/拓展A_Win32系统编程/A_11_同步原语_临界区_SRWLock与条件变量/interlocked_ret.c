/* 互锁函数的返回值：Increment 返回新值，ExchangeAdd 与 CompareExchange 返回旧值。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    LONG x = 5;
    LONG ret;

    printf("x = %ld\n", x);

    ret = InterlockedIncrement(&x);
    printf("InterlockedIncrement(&x)        -> %ld, x = %ld\n", ret, x);

    ret = InterlockedExchangeAdd(&x, 3);
    printf("InterlockedExchangeAdd(&x, 3)   -> %ld, x = %ld\n", ret, x);

    ret = InterlockedCompareExchange(&x, 100, 99);
    printf("InterlockedCompareExchange(99)  -> %ld, x = %ld\n", ret, x);

    ret = InterlockedCompareExchange(&x, 100, 9);
    printf("InterlockedCompareExchange(9)   -> %ld, x = %ld\n", ret, x);
    return 0;
}