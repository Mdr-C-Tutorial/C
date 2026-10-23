#define _GNU_SOURCE
#include <stdio.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

int main(void) {
    struct timespec ts;
    for (int i = 0; i < 1000; i++) {
        if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts) == -1) {
            perror("clock_gettime");
            return 1;
        }
    }
    printf("clock_gettime 系统调用执行了 1000 次\n");
    return 0;
}
