#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/* 打印 /proc/self/fdinfo 里的 pos 和 flags，比较 O_CLOEXEC 的影响 */
static void show(const char *tag, int fd) {
    char path[64];
    snprintf(path, sizeof path, "/proc/self/fdinfo/%d", fd);
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror(path);
        return;
    }
    char line[128];
    printf("[%s] fd=%d\n", tag, fd);
    while (fgets(line, sizeof line, fp) != NULL) {
        if (line[0] == 'p' || line[0] == 'f') {
            printf("  %s", line);
        }
    }
    fclose(fp);
}

int main(void) {
    int plain = open("plain2.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int clo = open("clo2.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (plain == -1 || clo == -1) {
        perror("open");
        return 1;
    }
    show("不带 O_CLOEXEC", plain);
    show("带 O_CLOEXEC", clo);
    return 0;
}