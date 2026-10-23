#define _POSIX_C_SOURCE 200809L

#include <poll.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* 错误示范：poll 等到标准输入可读，然后用 fgets 读一行。 */
int main(void) {
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
    char line[256];
    for (;;) {
        if (poll(&pfd, 1, -1) < 0) {
            return 1;
        }
        if (fgets(line, sizeof line, stdin) == NULL) {
            break;
        }
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        double s = (double)(t.tv_sec - t0.tv_sec) +
                   (double)(t.tv_nsec - t0.tv_nsec) / 1e9;
        printf("%.1fs got: %s", s, line);
        fflush(stdout);
    }
    return 0;
}
