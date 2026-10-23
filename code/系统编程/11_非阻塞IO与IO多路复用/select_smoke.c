#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
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
    printf("读端 fd = %d\n", p[0]);

    for (int round = 0; round < 2; round++) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(p[0], &readfds);
        struct timeval tv = {.tv_sec = 0, .tv_usec = 300000};
        int ready = select(p[0] + 1, &readfds, NULL, NULL, &tv);
        if (ready == -1) {
            die("select");
        }
        printf("第 %d 轮：select 返回 %d，FD_ISSET(p[0]) = %d\n", round + 1,
               ready, FD_ISSET(p[0], &readfds));
        if (round == 0) {
            if (write(p[1], "hello", 5) != 5) {
                die("write");
            }
        }
    }
    char buf[8];
    ssize_t n = read(p[0], buf, sizeof buf);
    printf("读回 %zd 字节：%.*s\n", n, (int)n, buf);
    return 0;
}
