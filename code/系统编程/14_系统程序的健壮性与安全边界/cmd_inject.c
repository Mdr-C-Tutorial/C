/* 命令注入演示：用 system() 把文件名拼进命令行，让 wc 统计行数。
 * 只调用 wc 与 echo，不执行任何有副作用的命令。 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s filename\n", argv[0]);
        return 1;
    }
    char cmd[512];
    snprintf(cmd, sizeof cmd, "wc -l %s", argv[1]);
    printf("$ %s\n", cmd);
    fflush(stdout);
    int rc = system(cmd);
    printf("system returned %d\n", WEXITSTATUS(rc));
    return 0;
}