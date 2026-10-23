#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 把链接目标里属于当前目录的前缀换成 .，输出短一些 */
static void print_target(const char *target) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof cwd) != NULL) {
        size_t len = strlen(cwd);
        if (strncmp(target, cwd, len) == 0 && target[len] == '/') {
            printf(".%s", target + len);
            return;
        }
    }
    printf("%s", target);
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY | O_CLOEXEC);
        if (fd == -1) {
            die(argv[i]);
        }
        printf("open(\"%s\") = %d\n", argv[i], fd);
    }

    for (int fd = 0; fd < 16; fd++) {
        char link[32];
        char target[PATH_MAX];
        snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
        ssize_t n = readlink(link, target, sizeof target - 1);
        if (n == -1) {
            continue; /* 这个编号当前没有打开 */
        }
        target[n] = '\0';
        printf("%d -> ", fd);
        print_target(target);
        putchar('\n');
    }
    return 0;
}