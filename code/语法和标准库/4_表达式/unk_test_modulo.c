#include <stdio.h>

int main() {
    int a = 10;
    int b = 3;
    int c = -10;
    int d = -3;

    printf("%d %% %d = %d\n", a, b, a % b); // 10 % 3 = 1

    printf("%d %% %d = %d\n", c, d, c % d); // -10 % -3 = -1

    printf("%d %% %d = %d\n", a, d, a % d); // 10 % -3 = 1
    printf("%d %% %d = %d\n", c, b, c % b); // -10 % 3 = -1

    return 0;
}