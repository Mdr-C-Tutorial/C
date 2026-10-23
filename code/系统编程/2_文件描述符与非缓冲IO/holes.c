#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

int main(void) {
    int fd = open("holes.bin", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd == -1) {
        die("open");
    }
    if (write_all(fd, "head", 4) == -1) {
        die("write");
    }
    if (lseek(fd, 1024 * 1024, SEEK_SET) == -1) { /* 跳到 1 MiB 处 */
        die("lseek");
    }
    if (write_all(fd, "tail", 4) == -1) {
        die("write");
    }
    if (close(fd) == -1) {
        die("close");
    }
    return 0;
}