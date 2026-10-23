#include <stdio.h>

int main(void) {
    int n = 42;
    printf("%d\n", doubled(n));
    return 0;
}

int doubled(int n) {
    return n * 2;
}
