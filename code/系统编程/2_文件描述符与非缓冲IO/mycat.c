#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUF_SIZE 65536

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 把 buf 开始的 len 字节全部写到 fd。成功返回 0；失败返回 -1，errno 指明原因 */
int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n == -1) {
            if (errno == EINTR) {
                continue; /* 被信号打断，重新写（第 6 章） */
            }
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static void copy_fd(int in, const char *name) {
    static char buf[BUF_SIZE];
    for (;;) {
        ssize_t n = read(in, buf, sizeof buf);
        if (n == 0) {
            return; /* 文件结束 */
        }
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            die(name);
        }
        if (write_all(STDOUT_FILENO, buf, (size_t)n) == -1) {
            die("write");
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        copy_fd(STDIN_FILENO, "stdin");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY | O_CLOEXEC);
        if (fd == -1) {
            die(argv[i]);
        }
        copy_fd(fd, argv[i]);
        if (close(fd) == -1) {
            die("close");
        }
    }
    return 0;
}