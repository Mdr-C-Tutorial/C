/* 用 strace 注入 write 的错误，检查程序是否处理了写失败。
 * 正常的复制程序，遇到 ENOSPC 应当报错退出，而不是静默留下半个文件。 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t done = 0;
    while (done < len) {
        ssize_t n = write(fd, p + done, len - done);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s SRC DST\n", argv[0]);
        return 1;
    }
    int in = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (in == -1) {
        perror("open src");
        return 1;
    }
    int out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out == -1) {
        perror("open dst");
        return 1;
    }
    char buf[4096];
    for (;;) {
        ssize_t n = read(in, buf, sizeof buf);
        if (n == 0) {
            break;
        }
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            return 1;
        }
        if (write_all(out, buf, (size_t)n) == -1) {
            perror("write");
            return 1;
        }
    }
    if (close(out) == -1) {
        perror("close");
        return 1;
    }
    close(in);
    return 0;
}