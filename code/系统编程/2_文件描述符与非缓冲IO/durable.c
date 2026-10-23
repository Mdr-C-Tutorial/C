#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 以 0600 权限新建 secret.txt，用 stdio 写入，确认数据到达存储设备后再关闭 */
int main(void) {
    int fd = open("secret.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd == -1) {
        die("open");
    }
    FILE *fp = fdopen(fd, "w"); /* 在已有的描述符上建立流 */
    if (fp == NULL) {
        die("fdopen");
    }

    fprintf(fp, "token=%d\n", 42);
    if (fflush(fp) == EOF) { /* stdio 缓冲区 → 内核 */
        die("fflush");
    }
    if (fsync(fileno(fp)) == -1) { /* 内核 → 存储设备 */
        die("fsync");
    }
    if (fclose(fp) == EOF) { /* 同时关闭 fd */
        die("fclose");
    }
    return 0;
}