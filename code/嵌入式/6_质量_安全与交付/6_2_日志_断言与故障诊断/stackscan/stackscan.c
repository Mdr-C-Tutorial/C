/*
 * stackscan.c：从一段栈内存里找出可能的返回地址，还原调用链。
 *
 * 用法：
 *   stackscan <elf 文件> <栈转储文件> [--text-start 0x...]
 *
 * 栈转储文件是主机上生成的二进制，每个字 4 字节、小端。
 * 工具从 ELF 里读出 .text 的地址与长度，凡是落在 .text 范围内、
 * 且最低位为 1 的值都当作候选的 Thumb 返回地址打印出来。
 * 这一步只做筛选，地址到函数名的翻译交给 addr2line。
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 只支持小端 32 位 ARM ELF，足够读本教程交叉编译出来的文件 */
struct text_range {
    uint32_t addr;
    uint32_t size;
};

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* 在 ELF 的段头表里找 .text：按名字查节名字符串表 */
static int find_text(const uint8_t *elf, size_t len, struct text_range *out) {
    if (len < 52 || elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' ||
        elf[3] != 'F') {
        fprintf(stderr, "不是 ELF 文件\n");
        return -1;
    }
    if (elf[4] != 1 || elf[5] != 1) {
        fprintf(stderr, "只支持 32 位小端 ELF\n");
        return -1;
    }

    uint32_t shoff = read_u32(elf + 32);
    uint16_t shentsize = read_u16(elf + 46);
    uint16_t shnum = read_u16(elf + 48);
    uint16_t shstrndx = read_u16(elf + 50);
    if (shoff == 0 || shnum == 0 || shstrndx >= shnum) {
        fprintf(stderr, "ELF 里没有节头表\n");
        return -1;
    }

    const uint8_t *shstr = elf + shoff + (size_t)shstrndx * shentsize;
    uint32_t str_off = read_u32(shstr + 16);

    for (uint16_t i = 0; i < shnum; ++i) {
        const uint8_t *sh = elf + shoff + (size_t)i * shentsize;
        uint32_t name = read_u32(sh + 0);
        uint32_t addr = read_u32(sh + 12);
        uint32_t size = read_u32(sh + 20);
        const char *s = (const char *)(elf + str_off + name);
        if (strcmp(s, ".text") == 0) {
            out->addr = addr;
            out->size = size;
            return 0;
        }
    }
    fprintf(stderr, "找不到 .text 节\n");
    return -1;
}

static uint8_t *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "打不开 %s: %s\n", path, strerror(errno));
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    uint8_t *buf = malloc((size_t)n);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len_out = (size_t)n;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "用法：%s <elf> <stack.bin>\n", argv[0]);
        return 2;
    }

    size_t elf_len = 0;
    uint8_t *elf = read_file(argv[1], &elf_len);
    if (elf == NULL) {
        return 1;
    }

    struct text_range text;
    if (find_text(elf, elf_len, &text) != 0) {
        free(elf);
        return 1;
    }
    free(elf);

    size_t dump_len = 0;
    uint8_t *dump = read_file(argv[2], &dump_len);
    if (dump == NULL) {
        return 1;
    }

    printf(".text: 0x%08lX..0x%08lX (%lu bytes)\n", (unsigned long)text.addr,
           (unsigned long)(text.addr + text.size), (unsigned long)text.size);
    printf("dump : %lu bytes, %lu words\n", (unsigned long)dump_len,
           (unsigned long)(dump_len / 4u));

    int found = 0;
    for (size_t off = 0; off + 4u <= dump_len; off += 4u) {
        uint32_t v = read_u32(dump + off);
        if ((v & 1u) == 0u) {
            continue; /* Thumb 地址最低位为 1 */
        }
        uint32_t a = v & ~1u;
        if (a >= text.addr && a < text.addr + text.size) {
            printf("offset %4lu: 0x%08lX  -> 0x%08lX\n", (unsigned long)off,
                   (unsigned long)v, (unsigned long)a);
            found++;
        }
    }

    printf("%d candidate return address(es)\n", found);
    free(dump);
    return found > 0 ? 0 : 1;
}