#include <stdio.h>

void say_hello() {
    puts("Hello, world!\n");
}

void print_add(int a, int b) {
    printf("%d + %d = %d\n", a, b, a + b);
}

int add(int a, int b) {
    return a + b;
}

int main(void) {
    say_hello();
    print_add(1, 2);
    printf("1 + 2 = %d\n", add(1, 2));
    return 0;
}