#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int do_write = argc > 1 && strcmp(argv[1], "write") == 0;
    long page = sysconf(_SC_PAGESIZE);
    char *p = mmap(NULL, (size_t)page, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    p[0] = 'A';
    printf("写入前：p[0] = %c\n", p[0]);
    fflush(stdout);

    if (mprotect(p, (size_t)page, PROT_READ) == -1) {
        perror("mprotect");
        return EXIT_FAILURE;
    }
    printf("已改成只读，准备写入\n");
    fflush(stdout);

    if (do_write) {
        p[0] = 'B';
        printf("这行不会执行：p[0] = %c\n", p[0]);
    }
    printf("没有写入，正常退出\n");
    munmap(p, (size_t)page);
    return 0;
}
