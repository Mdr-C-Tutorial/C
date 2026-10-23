#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>

/* 对管道调用 lseek，看它怎样失败 */
int main(void) {
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }
    if (lseek(fds[0], 0, SEEK_CUR) == -1) {
        perror("lseek");
    }
    return 0;
}