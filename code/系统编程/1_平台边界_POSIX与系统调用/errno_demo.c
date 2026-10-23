#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    ssize_t n = write(-1, "x", 1);
    int err = errno; /* 马上保存：后面的调用也可能改动 errno */
    printf("write 返回 %zd，errno = %d（%s）\n", n, err, strerror(err));

    pid_t pid = getpid();
    printf("getpid 返回 %d，errno 仍是 %d\n", (int)pid, errno);
    return 0;
}
