#include <stdio.h>

static int square(int x) {
    return x * x;
}

static int sum_squares(int n) {
    int total = 0;
    for (int i = 1; i <= n; i++) {
        total += square(i);
    }
    return total;
}

static int collatz_steps(int n) {
    int steps = 0;
    while (n != 1) {
        n = (n % 2 == 0) ? n / 2 : 3 * n + 1;
        steps++;
    }
    return steps;
}

int main(void) {
    printf("%d\n", sum_squares(100));
    printf("%d\n", collatz_steps(27));
    return 0;
}
