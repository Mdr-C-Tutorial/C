#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 用法：./newfile 文件名。文件已存在时失败 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "用法：%s 文件名\n", argv[0]);
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd == -1) {
        die(argv[1]);
    }
    printf("新建了 %s，描述符 %d\n", argv[1], fd);
    if (close(fd) == -1) {
        die("close");
    }
    return 0;
}