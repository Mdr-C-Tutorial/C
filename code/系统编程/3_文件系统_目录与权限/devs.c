#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        struct stat st;

        if (stat(argv[i], &st) == -1) {
            perror(argv[i]);
            continue;
        }
        printf("%-10s dev=%u,%u ino=%llu\n", argv[i], major(st.st_dev),
               minor(st.st_dev), (unsigned long long)st.st_ino);
    }
    return 0;
}
