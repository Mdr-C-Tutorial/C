#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    errno = 0;
    write(-1, "x", 1); /* 失败，errno 变成 EBADF */

    pid_t pid = getpid(); /* 这一次成功了，errno 却没有被清掉 */
    if (errno != 0) {
        perror("getpid");
    }
    printf("getpid 返回 %d\n", (int)pid);
    return 0;
}
