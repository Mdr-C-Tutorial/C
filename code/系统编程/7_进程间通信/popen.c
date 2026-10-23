#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(void) {
    FILE *fp = popen("wc -l", "w");
    if (fp == NULL) {
        perror("popen");
        return EXIT_FAILURE;
    }
    for (int i = 1; i <= 5; i++) {
        fprintf(fp, "line %d\n", i);
    }
    int status = pclose(fp);
    if (status == -1) {
        perror("pclose");
        return EXIT_FAILURE;
    }
    printf("wc 退出状态：%d\n", WEXITSTATUS(status));
    return EXIT_SUCCESS;
}
