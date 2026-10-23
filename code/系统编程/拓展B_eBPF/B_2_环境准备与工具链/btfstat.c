#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct btf_header {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint32_t hdr_len;
    uint32_t type_off;
    uint32_t type_len;
    uint32_t str_off;
    uint32_t str_len;
};

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/sys/kernel/btf/vmlinux";
    struct btf_header h;
    struct stat st;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return 1;
    }
    if (fstat(fd, &st) != 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        close(fd);
        return 1;
    }
    if (read(fd, &h, sizeof h) != (ssize_t)sizeof h) {
        fprintf(stderr, "%s: file shorter than a BTF header\n", path);
        close(fd);
        return 1;
    }
    close(fd);

    if (h.magic != 0xeb9f || h.version != 1) {
        fprintf(stderr, "%s: not a BTF blob (magic 0x%04x, version %u)\n",
                path, h.magic, h.version);
        return 1;
    }
    printf("BTF v%u: types %u B at %u, strings %u B at %u, file %lld B\n",
           h.version, h.type_len, h.type_off, h.str_len, h.str_off,
           (long long)st.st_size);
    return 0;
}
