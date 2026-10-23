#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *type_name(mode_t mode) {
    if (S_ISREG(mode)) {
        return "regular file";
    }
    if (S_ISDIR(mode)) {
        return "directory";
    }
    if (S_ISLNK(mode)) {
        return "symbolic link";
    }
    if (S_ISFIFO(mode)) {
        return "FIFO";
    }
    if (S_ISSOCK(mode)) {
        return "socket";
    }
    if (S_ISCHR(mode)) {
        return "character device";
    }
    if (S_ISBLK(mode)) {
        return "block device";
    }
    return "unknown";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s PATH... | %s -\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i++) {
        struct stat st;
        const char *label;

        if (strcmp(argv[i], "-") == 0) {
            label = "<stdin>";
            if (fstat(STDIN_FILENO, &st) == -1) {
                perror("fstat");
                return EXIT_FAILURE;
            }
        } else {
            label = argv[i];
            if (lstat(argv[i], &st) == -1) {
                perror(argv[i]);
                return EXIT_FAILURE;
            }
        }
        printf("%-14s %s\n", label, type_name(st.st_mode));
    }
    return 0;
}
