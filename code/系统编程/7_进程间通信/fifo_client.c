#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s FIFO ID RECLEN COUNT\n", argv[0]);
        return EXIT_FAILURE;
    }
    int id = atoi(argv[2]);
    size_t reclen = (size_t)atoi(argv[3]);
    int count = atoi(argv[4]);

    int fd = open(argv[1], O_WRONLY);
    if (fd == -1) {
        die("open");
    }

    char *rec = malloc(reclen);
    if (rec == NULL) {
        die("malloc");
    }
    for (int seq = 0; seq < count; seq++) {
        int head = snprintf(rec, reclen, "%02d:%04d:", id, seq);
        memset(rec + head, 'A' + id, reclen - 1 - (size_t)head);
        rec[reclen - 1] = '\n';
        ssize_t n = write(fd, rec, reclen);
        if (n != (ssize_t)reclen) {
            die("write");
        }
    }
    free(rec);
    close(fd);
    return EXIT_SUCCESS;
}
