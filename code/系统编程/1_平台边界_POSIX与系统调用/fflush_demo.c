#include <stdio.h>

int main(void) {
    printf("line 1\n");
    fflush(stdout);
    printf("line 2\n");
    fflush(stdout);
    printf("line 3\n");
    fflush(stdout);
    return 0;
}
