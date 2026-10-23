#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* 两页的匿名映射，只把第二页改成只读 */
int main(int argc, char *argv[]) {
    int do_write = argc > 1 && strcmp(argv[1], "write") == 0;
    long page = sysconf(_SC_PAGESIZE);
    char *p = mmap(NULL, (size_t)page * 2, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    p[0] = 'a';
    p[page] = 'b';

    if (mprotect(p + page, (size_t)page, PROT_READ) == -1) {
        perror("mprotect");
        return EXIT_FAILURE;
    }
    printf("第一页可写：%c\n", p[0]);
    fflush(stdout);
    p[0] = 'A';
    printf("第一页改成 %c，仍然可写\n", p[0]);
    fflush(stdout);

    if (do_write) {
        printf("尝试写第二页\n");
        fflush(stdout);
        p[page] = 'B';
    }
    printf("第二页读到 %c\n", p[page]);
    munmap(p, (size_t)page * 2);
    return 0;
}
