#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void show(const char *name, int fd) {
    int fl = fcntl(fd, F_GETFL);
    if (fl == -1) {
        die("fcntl");
    }
    printf("%s（描述符 %d）：%s\n", name, fd, (fl & O_APPEND) ? "带 O_APPEND" : "不带 O_APPEND");
}

int main(void) {
    /* 第一个打开文件描述，再用 dup 复制出一个描述符 */
    int a = open("flags.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (a == -1) {
        die("open");
    }
    int b = dup(a);
    if (b == -1) {
        die("dup");
    }

    /* 第二个打开文件描述：同一个文件，另外 open 一次 */
    int c = open("flags.txt", O_WRONLY | O_CLOEXEC);
    if (c == -1) {
        die("open");
    }

    show("a", a);
    show("b（dup 自 a）", b);
    show("c（另外 open 一次）", c);

    int fl = fcntl(b, F_GETFL);
    if (fl == -1 || fcntl(b, F_SETFL, fl | O_APPEND) == -1) {
        die("fcntl");
    }
    printf("通过 b 加上 O_APPEND 之后：\n");
    show("a", a);
    show("b（dup 自 a）", b);
    show("c（另外 open 一次）", c);
    return 0;
}