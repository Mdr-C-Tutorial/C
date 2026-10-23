/* 产生大量输出，用来验证父进程边读边等不会因为管道缓冲区满而死锁。 */
#include <stdio.h>

int main(void) {
    for (int i = 1; i <= 5000; i++) {
        printf("line %d\n", i);
        if (i % 1000 == 0) {
            fprintf(stderr, "progress %d\n", i);
        }
    }
    return 0;
}