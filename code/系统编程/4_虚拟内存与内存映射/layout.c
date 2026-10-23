#define _POSIX_C_SOURCE 200809L

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int g_init = 42;  /* .data：有初值的全局对象 */
int g_bss;        /* .bss：没有初值的全局对象 */
static int s_bss; /* .bss：没有初值的静态对象 */

/* 去掉 maps 行里属于当前目录的前缀，路径显示成 ./layout */
static void shorten_path(char *line) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof cwd) == NULL) {
        return;
    }
    size_t len = strlen(cwd);
    char *p = strstr(line, cwd);
    if (p != NULL && p[len] == '/') {
        memmove(p + 1, p + len, strlen(p + len) + 1);
        *p = '.';
    }
}

/* 在 /proc/self/maps 里找包含 addr 的那一行并打印 */
static void show_mapping(const char *what, const void *addr) {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    char line[512];
    while (fgets(line, sizeof line, fp) != NULL) {
        unsigned long start;
        unsigned long end;
        if (sscanf(line, "%lx-%lx", &start, &end) != 2) {
            continue;
        }
        unsigned long a = (unsigned long)addr;
        if (a >= start && a < end) {
            shorten_path(line);
            printf("%-8s %p  %s", what, addr, line);
            break;
        }
    }
    fclose(fp);
}

/* 打印完整的 maps，路径同样缩短 */
static void dump_all(void) {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    char line[512];
    while (fgets(line, sizeof line, fp) != NULL) {
        shorten_path(line);
        fputs(line, stdout);
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    int local = 7;
    void *small = malloc(1000);
    void *big = malloc(1024 * 1024);
    if (small == NULL || big == NULL) {
        return EXIT_FAILURE;
    }

    show_mapping("main", (const void *)main);
    show_mapping("g_init", &g_init);
    show_mapping("g_bss", &g_bss);
    show_mapping("s_bss", &s_bss);
    show_mapping("small", small);
    show_mapping("big", big);
    show_mapping("local", &local);

    if (argc > 1 && strcmp(argv[1], "-a") == 0) {
        dump_all();
    }

    free(big);
    free(small);
    return 0;
}
