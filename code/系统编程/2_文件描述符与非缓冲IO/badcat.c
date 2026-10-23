#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 故意保留两处错误：把 read 的缓冲区当字符串用；不看 write 写出了多少 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "用法：%s 文件\n", argv[0]);
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    char buf[4096];
    while (read(fd, buf, sizeof buf) > 0) {
        write(STDOUT_FILENO, buf, strlen(buf));
    }
    close(fd);
    return 0;
}