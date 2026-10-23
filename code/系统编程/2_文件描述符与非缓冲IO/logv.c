#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 时间戳、消息和换行符分别在三块内存里，一次 writev 写出 */
static void log_line(int fd, const char *msg) {
    char stamp[32];
    time_t now = time(NULL);
    struct tm tm;
    if (localtime_r(&now, &tm) == NULL) {
        die("localtime_r");
    }
    size_t stamp_len = strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S ", &tm);

    struct iovec iov[3] = {
        {.iov_base = stamp, .iov_len = stamp_len},
        {.iov_base = (char *)msg, .iov_len = strlen(msg)},
        {.iov_base = "\n", .iov_len = 1},
    };
    if (writev(fd, iov, 3) == -1) {
        die("writev");
    }
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        log_line(STDOUT_FILENO, argv[i]);
    }
    return 0;
}