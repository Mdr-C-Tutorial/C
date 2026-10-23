/* 同样的功能，用 posix_spawnp 直接传 argv，不经过 shell。
 * 文件名整体作为一个参数交给 wc，前面加 -- 防止以 - 开头的名字被当成选项。 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s filename\n", argv[0]);
        return 1;
    }
    char *child_argv[] = {"wc", "-l", "--", argv[1], NULL};
    printf("$ wc -l -- '%s'\n", argv[1]);
    fflush(stdout);
    pid_t pid;
    int rc = posix_spawnp(&pid, "wc", NULL, NULL, child_argv, environ);
    if (rc != 0) {
        fprintf(stderr, "posix_spawnp: %s\n", strerror(rc));
        return 1;
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }
    printf("child exit=%d\n", WEXITSTATUS(status));
    return 0;
}