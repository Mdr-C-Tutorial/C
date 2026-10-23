#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int count_entry(const char *path, const struct stat *sb, int type,
                       struct FTW *ftw) {
    (void)path;
    (void)sb;
    (void)type;
    (void)ftw;
    return 0;
}

int main(int argc, char *argv[]) {
    char *line = NULL;
    size_t cap = 0;
    if (getline(&line, &cap, stdin) == -1) {
        return 1;
    }
    char *copy = strdup(line);
    if (copy == NULL) {
        perror("strdup");
        return 1;
    }
    int fd = fileno(stdin);
    int entries = 0;
    if (argc > 1 && nftw(argv[1], count_entry, 16, FTW_PHYS) == 0) {
        entries = 1;
    }
    printf("fd=%d entries=%d copy=%s", fd, entries, copy);
    free(copy);
    free(line);
    return 0;
}
