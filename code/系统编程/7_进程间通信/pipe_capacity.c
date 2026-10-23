#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    int fds[2];
    if (pipe(fds) == -1) {
        die("pipe");
    }
    int flags = fcntl(fds[1], F_GETFL);
    if (flags == -1) {
        die("F_GETFL");
    }
    if (fcntl(fds[1], F_SETFL, flags | O_NONBLOCK) == -1) {
        die("F_SETFL");
    }

    char buf[4096] = {0};
    long total = 0;
    for (;;) {
        ssize_t n = write(fds[1], buf, sizeof buf);
        if (n == -1) {
            if (errno == EAGAIN) {
                break;
            }
            die("write");
        }
        total += n;
    }
    printf("管道写满时容纳了 %ld 字节\n", total);
    close(fds[0]);
    close(fds[1]);
    return EXIT_SUCCESS;
}
