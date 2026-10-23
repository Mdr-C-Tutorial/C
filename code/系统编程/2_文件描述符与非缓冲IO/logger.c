#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LINES 100000

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

/* 用法：./logger seek|append 标签。向 log.txt 追加 LINES 行 */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法：%s seek|append 标签\n", argv[0]);
        return EXIT_FAILURE;
    }
    int use_append = strcmp(argv[1], "append") == 0;
    int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
    if (use_append) {
        flags |= O_APPEND;
    }
    int fd = open("log.txt", flags, 0644);
    if (fd == -1) {
        die("open");
    }

    char line[64];
    for (int i = 0; i < LINES; i++) {
        int len = snprintf(line, sizeof line, "%s %d\n", argv[2], i);
        if (!use_append && lseek(fd, 0, SEEK_END) == -1) { /* 先定位到末尾…… */
            die("lseek");
        }
        if (write_all(fd, line, (size_t)len) == -1) { /* ……再写 */
            die("write");
        }
    }
    if (close(fd) == -1) {
        die("close");
    }
    return 0;
}