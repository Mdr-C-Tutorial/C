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

/* 用法：./copybuf 缓冲区字节数 源文件 目标文件 */
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "用法：%s 缓冲区字节数 源文件 目标文件\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t size = strtoul(argv[1], NULL, 10);
    char *buf = malloc(size);
    if (size == 0 || buf == NULL) {
        fprintf(stderr, "缓冲区大小无效\n");
        return EXIT_FAILURE;
    }

    int in = open(argv[2], O_RDONLY | O_CLOEXEC);
    if (in == -1) {
        die(argv[2]);
    }
    int out = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out == -1) {
        die(argv[3]);
    }

    for (;;) {
        ssize_t n = read(in, buf, size);
        if (n == 0) {
            break;
        }
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("read");
        }
        if (write_all(out, buf, (size_t)n) == -1) {
            die("write");
        }
    }

    if (close(out) == -1) {
        die("close");
    }
    close(in);
    free(buf);
    return 0;
}