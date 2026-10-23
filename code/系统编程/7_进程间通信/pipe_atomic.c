#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define WRITERS 8
#define RECORDS 500

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 写端：每条记录 RECLEN 字节，整条只用一种字母填满 */
static void writer(int fd, int id, size_t reclen) {
    char *rec = malloc(reclen);
    if (rec == NULL) {
        die("malloc");
    }
    for (int seq = 0; seq < RECORDS; seq++) {
        int head = snprintf(rec, reclen, "%02d:%04d:", id, seq);
        memset(rec + head, 'A' + id, reclen - 1 - (size_t)head);
        rec[reclen - 1] = '\n';
        if (write(fd, rec, reclen) != (ssize_t)reclen) {
            die("write");
        }
    }
    free(rec);
    _exit(EXIT_SUCCESS);
}

static int record_ok(const char *p, size_t len) {
    if (p[2] != ':' || p[7] != ':' || p[len - 1] != '\n') {
        return 0;
    }
    char want = (char)('A' + (p[0] - '0') * 10 + (p[1] - '0'));
    for (size_t k = 8; k + 1 < len; k++) {
        if (p[k] != want) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s RECLEN\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t reclen = (size_t)atol(argv[1]);

    int fds[2];
    if (pipe(fds) == -1) {
        die("pipe");
    }
    for (int i = 0; i < WRITERS; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            die("fork");
        }
        if (pid == 0) {
            close(fds[0]);
            writer(fds[1], i, reclen);
        }
    }
    close(fds[1]);

    char buf[1 << 20];
    size_t used = 0;
    int got = 0;
    int good = 0;
    while (got < WRITERS * RECORDS) {
        ssize_t n = read(fds[0], buf + used, sizeof buf - used);
        if (n <= 0) {
            break;
        }
        used += (size_t)n;
        struct timespec ts = {0, 1000000L}; /* 每轮慢 1 毫秒 */
        nanosleep(&ts, NULL);
        while (used >= reclen) {
            if (record_ok(buf, reclen)) {
                good++;
            }
            got++;
            memmove(buf, buf + reclen, used - reclen);
            used -= reclen;
        }
    }
    for (int i = 0; i < WRITERS; i++) {
        if (wait(NULL) == -1) {
            break;
        }
    }
    printf("记录长度 %zu：共 %d 条，完整 %d 条\n", reclen, got, good);
    return EXIT_SUCCESS;
}
