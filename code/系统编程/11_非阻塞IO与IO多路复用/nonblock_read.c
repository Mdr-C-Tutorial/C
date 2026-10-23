#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void try_read(const char *label, int fd) {
    char buf[64];
    errno = 0;
    ssize_t n = read(fd, buf, sizeof buf);
    if (n == -1) {
        printf("%s：read 返回 -1，errno = %d（%s）\n", label, errno,
               strerror(errno));
    } else {
        printf("%s：read 返回 %zd，读到 \"%.*s\"\n", label, n, (int)n, buf);
    }
}

int main(void) {
    int p[2];
    if (pipe(p) == -1) {
        die("pipe");
    }
    if (set_nonblock(p[0]) == -1) {
        die("fcntl");
    }

    try_read("空管道", p[0]);
    if (write(p[1], "hello", 5) != 5) {
        die("write");
    }
    try_read("写入 5 字节后", p[0]);
    try_read("读空后再读", p[0]);
    return 0;
}