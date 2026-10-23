#define _XOPEN_SOURCE 700

#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static unsigned long long total_blocks;

static int on_file(const char *path, const struct stat *st, int type,
                   struct FTW *ftwbuf) {
    (void)path;
    (void)type;
    (void)ftwbuf;

    total_blocks += (unsigned long long)st->st_blocks;
    return 0;
}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : ".";

    /* FTW_PHYS：不跟随符号链接。 */
    if (nftw(path, on_file, 16, FTW_PHYS) == -1) {
        perror("nftw");
        return EXIT_FAILURE;
    }
    /* st_blocks 以 512 字节为单位，du 默认以 1 KiB 为单位。 */
    printf("%llu\t%s\n", total_blocks / 2, path);
    return 0;
}