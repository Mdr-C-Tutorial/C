#include <stdio.h>

int main(void) {
    int count = 0
    for (int i = 0; i < 3; i++) {
        count += i;
    }
    printf("%d\n", count);
    return 0;
}
