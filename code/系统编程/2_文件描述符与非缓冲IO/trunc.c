#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 用 O_TRUNC 打开 t.txt 并写入 4 个字节 */
int main(void) {
    int fd = open("t.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd == -1) {
        die("open");
    }
    if (write(fd, "new\n", 4) == -1) {
        die("write");
    }
    if (close(fd) == -1) {
        die("close");
    }
    return 0;
}