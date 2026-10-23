#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* 用法：./mapwrite shared|private 文件 */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法：%s shared|private 文件\n", argv[0]);
        return EXIT_FAILURE;
    }
    int shared = strcmp(argv[1], "shared") == 0;

    int fd = open(argv[2], O_RDWR | O_CLOEXEC);
    if (fd == -1) {
        perror(argv[2]);
        return EXIT_FAILURE;
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        return EXIT_FAILURE;
    }

    int flags = shared ? MAP_SHARED : MAP_PRIVATE;
    char *p = mmap(NULL, (size_t)st.st_size, PROT_READ | PROT_WRITE, flags, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    printf("映射前看到：%c\n", p[0]);
    p[0] = 'X';
    printf("改完后本进程看到：%c\n", p[0]);

    if (shared && msync(p, (size_t)st.st_size, MS_SYNC) == -1) {
        perror("msync");
        return EXIT_FAILURE;
    }
    if (munmap(p, (size_t)st.st_size) == -1) {
        perror("munmap");
        return EXIT_FAILURE;
    }
    close(fd);
    return 0;
}
