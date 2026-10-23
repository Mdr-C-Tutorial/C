#include <stdio.h>

static int add(int a, int b) {
    return a + b;
}

int main(void) {
    int total = add(1, 2);
    printf("%d\n", total);
    return 0;
}
