#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    FILE *fp = popen("ps -o comm= -p $$", "r");
    if (fp == NULL) {
        perror("popen");
        return EXIT_FAILURE;
    }
    char buf[128];
    if (fgets(buf, sizeof buf, fp) == NULL) {
        perror("fgets");
        pclose(fp);
        return EXIT_FAILURE;
    }
    printf("popen 实际启动的进程名：%s", buf);
    pclose(fp);
    return EXIT_SUCCESS;
}
