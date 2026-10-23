#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 列出本进程打开的描述符：编号、FD_CLOEXEC 标志、目标文件的名字。
 * 目标路径只取最后一段，避免把工作目录的完整路径打印出来。 */
int main(void) {
    DIR *dir = opendir("/proc/self/fd");
    if (dir == NULL) {
        perror("opendir");
        return 1;
    }
    int self = dirfd(dir);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char *end = NULL;
        long fd = strtol(entry->d_name, &end, 10);
        if (end == entry->d_name || *end != '\0' || fd == self) {
            continue;
        }
        char path[64];
        char target[512];
        snprintf(path, sizeof path, "/proc/self/fd/%ld", fd);
        ssize_t n = readlink(path, target, sizeof target - 1);
        if (n < 0) {
            continue;
        }
        target[n] = '\0';
        const char *base = strrchr(target, '/');
        base = base == NULL ? target : base + 1;
        int flags = fcntl((int)fd, F_GETFD);
        printf("fd %-2ld cloexec=%-2d %s\n", fd,
               flags >= 0 && (flags & FD_CLOEXEC) != 0, base);
    }
    closedir(dir);
    return 0;
}