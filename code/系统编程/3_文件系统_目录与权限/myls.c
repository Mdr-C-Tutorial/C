#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : ".";
    DIR *dir = opendir(path);
    if (dir == NULL) {
        die("opendir");
    }

    errno = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        printf("%-10llu %s\n", (unsigned long long)ent->d_ino, ent->d_name);
        errno = 0;
    }
    if (errno != 0) {
        die("readdir");
    }
    if (closedir(dir) == -1) {
        die("closedir");
    }
    return 0;
}
