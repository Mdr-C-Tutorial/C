/*
 * 第 13 章缺陷 (b)：新写的 load_file（fopen、fseek/ftell、malloc、fread）
 * 在一条直线错误路径上忘了 fclose。fseek 失败后直接返回 -1，FILE * 泄漏。
 *
 * 单独编译即可看到 -fanalyzer 的完整事件路径：
 *   gcc -std=c17 -Wall -Wextra -fanalyzer -c bugs/main_load_file_leak.c
 *
 * 注意：只有 load_file 返回 int、把缓冲区通过参数送出时，-fanalyzer
 * 才会在 GCC 13.2 与 14.2 上报告这条路径；若写成返回 char * 则实测不报。
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 读入 path 的全部内容。成功时返回 0，缓冲区由调用方 free，字节数写入 *len；
 * 失败时返回 -1 并设置 errno。
 */
static int load_file(const char *path, char **out, size_t *len) {
    FILE *in = fopen(path, "rb");
    if (in == NULL) {
        return -1;
    }

    if (fseek(in, 0, SEEK_END) != 0) {
        /* 缺陷：这里直接返回，没有 fclose(in) */
        return -1;
    }
    long size = ftell(in);
    if (size < 0) {
        fclose(in);
        return -1;
    }
    rewind(in);

    char *buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(in);
        return -1;
    }
    size_t got = fread(buf, 1, (size_t)size, in);
    fclose(in);
    buf[got] = '\0';
    *out = buf;
    *len = got;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s FILE\n", argv[0]);
        return 2;
    }
    char *text = NULL;
    size_t len = 0;
    if (load_file(argv[1], &text, &len) != 0) {
        fprintf(stderr, "cannot read %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    printf("%zu bytes\n", len);
    free(text);
    return 0;
}
