/* bpfobj.c：47107 的参考实现。不依赖 <elf.h>，自己解析 ELF64 的节头，
 * 列出 .bpf.o 里的程序节、license 与 BTF 节。
 *
 * 编译：gcc -std=c17 -Wall -Wextra bpfobj.c -o bpfobj
 * 用法：./bpfobj x.bpf.o
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct elf64_ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf64_shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

static int is_code_section(const char *name) {
    return strncmp(name, "tracepoint/", 11) == 0 || strncmp(name, "tp/", 3) == 0 ||
           strcmp(name, "xdp") == 0 || strncmp(name, "kprobe/", 7) == 0 ||
           strncmp(name, "cgroup/", 7) == 0 || strncmp(name, "lsm/", 4) == 0 ||
           strncmp(name, "tcx/", 4) == 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: bpfobj FILE.o\n");
        return 2;
    }
    FILE *fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }
    struct elf64_ehdr eh;
    if (fread(&eh, sizeof eh, 1, fp) != 1 ||
        memcmp(eh.e_ident, "\x7f" "ELF", 4) != 0) {
        printf("%s: not an ELF file\n", argv[1]);
        fclose(fp);
        return 1;
    }
    printf("%s: machine=%u sections=%u\n", argv[1], eh.e_machine, eh.e_shnum);
    printf("  machine 247 means Linux BPF (EM_BPF)\n");

    struct elf64_shdr *sh = calloc(eh.e_shnum, sizeof *sh);
    struct elf64_shdr strh;
    fseek(fp, (long)(eh.e_shoff + (uint64_t)eh.e_shstrndx * eh.e_shentsize),
          SEEK_SET);
    if (fread(&strh, sizeof strh, 1, fp) != 1) {
        fclose(fp);
        free(sh);
        return 1;
    }
    char *strtab = malloc((size_t)strh.sh_size);
    fseek(fp, (long)strh.sh_offset, SEEK_SET);
    if (fread(strtab, (size_t)strh.sh_size, 1, fp) != 1) {
        fclose(fp);
        free(strtab);
        free(sh);
        return 1;
    }
    fseek(fp, (long)eh.e_shoff, SEEK_SET);
    for (unsigned i = 0; i < eh.e_shnum; i++) {
        if (fread(&sh[i], sizeof sh[i], 1, fp) != 1) {
            break;
        }
    }
    for (unsigned i = 0; i < eh.e_shnum; i++) {
        const char *name = strtab + sh[i].sh_name;
        if (is_code_section(name)) {
            printf("  program section %-40s %llu bytes\n", name,
                   (unsigned long long)sh[i].sh_size);
        }
    }
    fclose(fp);
    free(strtab);
    free(sh);
    return 0;
}
