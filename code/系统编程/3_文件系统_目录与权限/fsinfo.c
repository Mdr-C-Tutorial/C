#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : ".";
    struct statvfs vfs;

    if (statvfs(path, &vfs) == -1) {
        die("statvfs");
    }

    unsigned long long block = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    printf("path            %s\n", path);
    printf("block size      %llu\n", block);
    printf("blocks          %llu\n", (unsigned long long)vfs.f_blocks);
    printf("free blocks     %llu\n", (unsigned long long)vfs.f_bfree);
    printf("available       %llu\n", (unsigned long long)vfs.f_bavail);
    printf("total bytes     %llu\n", block * (unsigned long long)vfs.f_blocks);
    printf("free bytes      %llu\n", block * (unsigned long long)vfs.f_bavail);
    printf("file name max   %lu\n", (unsigned long)vfs.f_namemax);
    return 0;
}
