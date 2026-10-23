#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 模拟存盘过程中断电：先以 O_TRUNC 打开目标文件，还没写任何内容就退出。 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s FILE\n", argv[0]);
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd == -1) {
        die("open");
    }
    printf("opened %s with O_TRUNC; exiting before write (simulated crash)\n",
           argv[1]);
    if (close(fd) == -1) {
        die("close");
    }
    return 0;
}
