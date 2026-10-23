#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* 三个 printf 和一次 write 混用；观察输出顺序与缓冲 */
int main(void) {
    printf("1. printf\n");
    if (write(STDOUT_FILENO, "2. write\n", 9) == -1) {
        perror("write");
        return EXIT_FAILURE;
    }
    printf("3. printf\n");
    return 0;
}