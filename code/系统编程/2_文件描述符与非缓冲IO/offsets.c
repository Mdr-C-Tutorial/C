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

static void put(int fd, const char *s, size_t len) {
    if (write_all(fd, s, len) == -1) {
        die("write");
    }
}

static void show(const char *name, int fd) {
    off_t pos = lseek(fd, 0, SEEK_CUR);
    if (pos == -1) {
        die("lseek");
    }
    printf("%s：描述符 %d，偏移量 %lld\n", name, fd, (long long)pos);
}

int main(void) {
    /* 情形一：同一个文件打开两次 */
    int a = open("twice.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    int b = open("twice.txt", O_WRONLY | O_CLOEXEC);
    if (a == -1 || b == -1) {
        die("open");
    }
    put(a, "hello,", 6);
    put(b, "world", 5);
    show("a", a);
    show("b", b);

    /* 情形二：打开一次，再用 dup 复制描述符 */
    int c = open("duped.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (c == -1) {
        die("open");
    }
    int d = dup(c);
    if (d == -1) {
        die("dup");
    }
    put(c, "hello,", 6);
    put(d, "world", 5);
    show("c", c);
    show("d", d);
    return 0;
}