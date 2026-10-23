#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define SIZE (4UL * 4096)

int main(void) {
    char *p = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    int nonzero = 0;
    for (size_t i = 0; i < SIZE; i++) {
        if (p[i] != 0) {
            nonzero++;
        }
    }
    printf("匿名映射 %lu 字节，非零字节数 %d\n", (unsigned long)SIZE, nonzero);

    strcpy(p, "hello anonymous");
    printf("写入后读出：%s\n", p);
    printf("munmap 返回 %d\n", munmap(p, SIZE));
    return 0;
}
