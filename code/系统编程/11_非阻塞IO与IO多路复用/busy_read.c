#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    int p[2];
    if (pipe(p) == -1) {
        die("pipe");
    }
    if (fcntl(p[0], F_SETFL, O_NONBLOCK) == -1) {
        die("fcntl");
    }

    struct timespec start;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        die("clock_gettime");
    }
    unsigned long calls = 0;
    for (;;) {
        char buf[64];
        ssize_t n = read(p[0], buf, sizeof buf);
        if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            die("read");
        }
        calls++;
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
            die("clock_gettime");
        }
        if (now.tv_sec - start.tv_sec >= 1) {
            break;
        }
    }
    printf("1 秒内 read 返回 EAGAIN %lu 次\n", calls);
    return 0;
}
