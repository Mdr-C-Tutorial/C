#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    if (close(STDIN_FILENO) == -1) {
        die("close");
    }

    int fd = open("greeting.txt", O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        die("open");
    }
    printf("open 返回 %d\n", fd);

    char line[64];
    if (fgets(line, sizeof line, stdin) != NULL) {
        printf("从描述符 0 读到：%s", line);
    }
    return 0;
}