#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("A\n");
    if (write(STDOUT_FILENO, "B\n", 2) == -1) {
        perror("write");
        return 1;
    }
    printf("C\n");
    return 0;
}
