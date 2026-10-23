#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* 去掉 maps 行里属于当前目录的前缀 */
static void shorten_path(char *line) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof cwd) == NULL) {
        return;
    }
    size_t len = strlen(cwd);
    char *q = strstr(line, cwd);
    if (q != NULL && q[len] == '/') {
        memmove(q + 1, q + len, strlen(q + len) + 1);
        *q = '.';
    }
}

/* 映射一个文件，打印它在 maps 里的那一行 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        return EXIT_FAILURE;
    }
    char *p = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    printf("文件大小 %lld 字节，映射到 %p\n", (long long)st.st_size, (void *)p);

    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        return EXIT_FAILURE;
    }
    char line[512];
    while (fgets(line, sizeof line, fp) != NULL) {
        unsigned long start;
        unsigned long end;
        if (sscanf(line, "%lx-%lx", &start, &end) != 2) {
            continue;
        }
        if ((unsigned long)p >= start && (unsigned long)p < end) {
            shorten_path(line);
            printf("%s", line);
            break;
        }
    }
    fclose(fp);
    munmap(p, (size_t)st.st_size);
    close(fd);
    return 0;
}
