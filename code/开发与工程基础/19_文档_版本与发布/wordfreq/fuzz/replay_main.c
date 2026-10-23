/* SPDX-License-Identifier: MIT */
/*
 * 模糊测试输入的重放程序：没有 clang 和 libFuzzer 的环境也能拿语料与崩溃输入
 * 做回归测试。每个文件参数是一个输入，全部运行一遍，全部正常则返回 0。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -Iinclude fuzz/replay_main.c \
 *           fuzz/fuzz_token.c src/token.c -o build/replay_token
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int replay_file(const char *path) {
    FILE *in = fopen(path, "rb");
    if (in == NULL) {
        fprintf(stderr, "replay: cannot open %s\n", path);
        return 1;
    }
    size_t cap = 4096;
    size_t used = 0;
    uint8_t *buf = malloc(cap);
    if (buf == NULL) {
        fclose(in);
        return 1;
    }
    for (;;) {
        if (used == cap) {
            uint8_t *bigger = realloc(buf, cap * 2);
            if (bigger == NULL) {
                free(buf);
                fclose(in);
                return 1;
            }
            buf = bigger;
            cap *= 2;
        }
        size_t n = fread(buf + used, 1, cap - used, in);
        used += n;
        if (n == 0) {
            break;
        }
    }
    fclose(in);

    LLVMFuzzerTestOneInput(buf, used);
    free(buf);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s FILE...\n", argv[0]);
        return 2;
    }
    for (int i = 1; i < argc; i++) {
        int status = replay_file(argv[i]);
        if (status != 0) {
            return status;
        }
    }
    return 0;
}
