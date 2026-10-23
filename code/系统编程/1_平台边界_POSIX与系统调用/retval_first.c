#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    if (write(-1, "x", 1) == -1) {
        perror("write");
    }
    pid_t pid = getpid();
    if (pid == -1) {
        perror("getpid");
    }
    printf("getpid 返回 %d\n", (int)pid);
    return 0;
}
