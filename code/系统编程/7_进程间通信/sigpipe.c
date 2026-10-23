#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int ignore = argc > 1 && strcmp(argv[1], "ignore") == 0;
    if (ignore) {
        struct sigaction sa = {0};
        sa.sa_handler = SIG_IGN;
        if (sigaction(SIGPIPE, &sa, NULL) == -1) {
            die("sigaction");
        }
    }

    int fds[2];
    if (pipe(fds) == -1) {
        die("pipe");
    }
    /* 读端立刻关掉，之后再写 */
    if (close(fds[0]) == -1) {
        die("close");
    }

    const char *msg = "nobody is listening\n";
    ssize_t n = write(fds[1], msg, strlen(msg));
    if (n == -1) {
        printf("write 失败：%s\n", errno == EPIPE ? "EPIPE" : strerror(errno));
        return EXIT_SUCCESS;
    }
    printf("write 返回 %zd\n", n);
    return EXIT_SUCCESS;
}
