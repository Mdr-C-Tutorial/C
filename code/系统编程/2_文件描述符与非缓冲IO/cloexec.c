#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 打开一个文件，再用 dup 复制一个不带 close-on-exec 的描述符，
   然后启动另一个程序，看它继承了哪些描述符 */
int main(void) {
    int drop = open("drop.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (drop == -1) {
        die("open");
    }
    int keep = dup(drop); /* 副本没有 FD_CLOEXEC */
    if (keep == -1) {
        die("dup");
    }
    printf("drop.txt -> %d（带 close-on-exec），副本 -> %d（不带）\n", drop, keep);
    if (fflush(stdout) == EOF) {
        die("fflush");
    }

    if (system("./fdlist") == -1) {
        die("system");
    }
    return 0;
}