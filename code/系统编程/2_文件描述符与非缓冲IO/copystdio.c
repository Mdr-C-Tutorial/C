#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 用法：./copystdio 源文件 目标文件。逐字节复制，缓冲交给 stdio */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法：%s 源文件 目标文件\n", argv[0]);
        return EXIT_FAILURE;
    }
    FILE *in = fopen(argv[1], "rb");
    if (in == NULL) {
        die(argv[1]);
    }
    FILE *out = fopen(argv[2], "wb");
    if (out == NULL) {
        die(argv[2]);
    }

    int c;
    while ((c = fgetc(in)) != EOF) {
        if (fputc(c, out) == EOF) {
            die("fputc");
        }
    }
    if (ferror(in)) {
        die("fgetc");
    }
    if (fclose(out) == EOF) {
        die("fclose");
    }
    fclose(in);
    return 0;
}