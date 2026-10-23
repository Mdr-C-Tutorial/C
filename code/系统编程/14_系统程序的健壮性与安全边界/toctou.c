/* TOCTOU 演示：先用 access() 检查，再 open()。
 * 两步之间文件名可以被换成指向另一个文件的符号链接。
 * 用法：./toctou PATH [sleep]
 * 带 sleep 时在检查和打开之间停 1 秒，方便在另一个终端里替换文件。 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s PATH [sleep]\n", argv[0]);
        return 1;
    }
    if (access(argv[1], W_OK) == -1) {
        fprintf(stderr, "access: %s\n", strerror(errno));
        return 1;
    }
    printf("access says writable: %s\n", argv[1]);
    if (argc == 3 && strcmp(argv[2], "sleep") == 0) {
        sleep(1);
    }
    int fd = open(argv[1], O_WRONLY | O_APPEND | O_CLOEXEC);
    if (fd == -1) {
        fprintf(stderr, "open: %s\n", strerror(errno));
        return 1;
    }
    static const char msg[] = "appended by toctou demo\n";
    if (write(fd, msg, sizeof msg - 1) == -1) {
        perror("write");
        return 1;
    }
    close(fd);
    printf("wrote to %s\n", argv[1]);
    return 0;
}