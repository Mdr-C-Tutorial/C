#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 第一次尝试：dup2 之前没有 fflush，stdio 缓冲区里的内容也跟着换了去向 */
int main(void) {
    printf("第 1 行：重定向之前\n");

    int fd = open("out.log", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd == -1) {
        die("open");
    }
    if (dup2(fd, STDOUT_FILENO) == -1) { /* 从此 1 号描述符指向 out.log */
        die("dup2");
    }
    if (close(fd) == -1) {
        die("close");
    }

    printf("第 2 行：重定向之后\n");
    return 0;
}