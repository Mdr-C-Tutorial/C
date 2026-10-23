#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : ".";

    /* 以目录本身为基准，之后的访问都用它的描述符加相对名字。 */
    int dfd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd == -1) {
        die("open");
    }
    DIR *dir = fdopendir(dfd);
    if (dir == NULL) {
        die("fdopendir");
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        struct stat st;

        if (fstatat(dfd, ent->d_name, &st, AT_SYMLINK_NOFOLLOW) == -1) {
            perror(ent->d_name);
            continue;
        }
        char type;
        if (S_ISDIR(st.st_mode)) {
            type = 'd';
        } else if (S_ISLNK(st.st_mode)) {
            type = 'l';
        } else if (S_ISREG(st.st_mode)) {
            type = '-';
        } else if (S_ISFIFO(st.st_mode)) {
            type = 'p';
        } else if (S_ISSOCK(st.st_mode)) {
            type = 's';
        } else {
            type = '?';
        }
        printf("%c %8lld  %s\n", type, (long long)st.st_size, ent->d_name);
    }
    if (closedir(dir) == -1) {
        die("closedir");
    }
    return 0;
}
