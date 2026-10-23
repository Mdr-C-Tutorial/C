#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
    const char *path = "victim.txt";
    struct stat st;

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd == -1) {
        die("open");
    }
    if (write_all(fd, "still here\n", 11) == -1) {
        die("write");
    }
    printf("opened %s as fd %d\n", path, fd);

    if (unlink(path) == -1) {
        die("unlink");
    }
    printf("unlinked %s\n", path);

    if (stat(path, &st) == -1) {
        printf("stat(%s) failed: %s\n", path, strerror(errno));
    }

    char linkpath[64];
    snprintf(linkpath, sizeof linkpath, "/proc/self/fd/%d", fd);
    char target[256];
    ssize_t n = readlink(linkpath, target, sizeof target - 1);
    if (n == -1) {
        die("readlink");
    }
    target[n] = '\0';
    /* 只留文件名，省得输出里出现整条工作目录路径。 */
    const char *base = strrchr(target, '/');
    printf("%s -> %s\n", linkpath, base ? base + 1 : target);

    char buf[64];
    ssize_t r = pread(fd, buf, sizeof buf - 1, 0);
    if (r == -1) {
        die("pread");
    }
    buf[r] = '\0';
    printf("read back: %s", buf);

    if (close(fd) == -1) {
        die("close");
    }
    printf("closed fd %d; the data is released now\n", fd);
    return 0;
}
