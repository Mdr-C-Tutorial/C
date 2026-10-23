#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main(void) {
    struct timespec ts;
    for (int i = 0; i < 1000; i++) {
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
            perror("clock_gettime");
            return 1;
        }
        (void)getpid();
    }
    printf("clock_gettime 与 getpid 各调用 1000 次\n");
    return 0;
}
