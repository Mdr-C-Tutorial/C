#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fifo_rec.h"

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 从 FIFO 读 COUNT 条定长记录，统计完整与损坏的条数 */
int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s FIFO RECLEN COUNT [DELAY_US]\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t reclen = (size_t)atoi(argv[2]);
    int want = atoi(argv[3]);
    long delay_us = argc > 4 ? atol(argv[4]) : 0;
    int good = 0;
    int bad = 0;
    int got = 0;
    char buf[65536];
    size_t used = 0;

    while (got < want) {
        int fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            die("open");
        }
        for (;;) {
            ssize_t n = read(fd, buf + used, sizeof buf - used);
            if (n == -1) {
                if (errno == EINTR) {
                    continue;
                }
                die("read");
            }
            if (n == 0) {
                break; /* 所有写端都已关闭 */
            }
            used += (size_t)n;
            while (used >= reclen && got < want) {
                if (record_ok(buf, reclen)) {
                    good++;
                } else {
                    bad++;
                }
                got++;
                memmove(buf, buf + reclen, used - reclen);
                used -= reclen;
            }
            if (got >= want) {
                break;
            }
            if (delay_us > 0) {
                struct timespec ts = {delay_us / 1000000,
                                      (delay_us % 1000000) * 1000};
                nanosleep(&ts, NULL);
            }
        }
        close(fd);
    }

    printf("收到 %d 条记录：完整 %d 条，损坏 %d 条\n", got, good, bad);
    return EXIT_SUCCESS;
}
