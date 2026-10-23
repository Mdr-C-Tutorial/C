#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORD_SIZE 64
#define NAME_SIZE 48

/* 定长记录：8 字节编号 + 48 字节名字 + 4 字节数值 + 4 字节填充 */
struct record {
    int64_t id;
    char name[NAME_SIZE];
    int32_t value;
    int32_t pad;
};

_Static_assert(sizeof(struct record) == RECORD_SIZE, "record must be 64 bytes");

/* 用法：./mkrec 文件 [条数]，默认 100 条 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法：%s 文件 [条数]\n", argv[0]);
        return EXIT_FAILURE;
    }
    long count = (argc > 2) ? strtol(argv[2], NULL, 10) : 100;

    FILE *fp = fopen(argv[1], "wb");
    if (fp == NULL) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    for (long i = 0; i < count; i++) {
        struct record r;
        memset(&r, 0, sizeof r);
        r.id = i;
        snprintf(r.name, sizeof r.name, "record-%03ld", i);
        r.value = (int32_t)(i * i % 1000);
        if (fwrite(&r, sizeof r, 1, fp) != 1) {
            perror("fwrite");
            return EXIT_FAILURE;
        }
    }
    if (fclose(fp) != 0) {
        perror("fclose");
        return EXIT_FAILURE;
    }
    printf("写入 %ld 条记录，共 %ld 字节\n", count, count * (long)sizeof(struct record));
    return 0;
}
