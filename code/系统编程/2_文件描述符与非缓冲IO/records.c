#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

#define RECORD_SIZE 6 /* 每条记录形如 "00042\n" */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "用法：%s 文件 记录号...\n", argv[0]);
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        die(argv[1]);
    }

    for (int i = 2; i < argc; i++) {
        long index = strtol(argv[i], NULL, 10);
        char rec[RECORD_SIZE];
        ssize_t n = pread(fd, rec, sizeof rec, (off_t)index * RECORD_SIZE);
        if (n == -1) {
            die("pread");
        }
        printf("记录 %ld：%.*s", index, (int)n, rec);
    }

    off_t pos = lseek(fd, 0, SEEK_CUR);
    if (pos == -1) {
        die("lseek");
    }
    printf("当前偏移量：%lld\n", (long long)pos);
    return 0;
}