#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void timestamp(char *buf, size_t len) {
    struct timespec ts;
    struct tm tm;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        die("clock_gettime");
    }
    if (localtime_r(&ts.tv_sec, &tm) == NULL) {
        die("localtime_r");
    }
    if (strftime(buf, len, "%H:%M:%S", &tm) == 0) {
        fprintf(stderr, "strftime: buffer too small\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    const char *dir = (argc > 1) ? argv[1] : ".";
    const char *suffix = (argc > 2) ? argv[2] : ".c";

    int fd = inotify_init1(IN_CLOEXEC);
    if (fd == -1) {
        die("inotify_init1");
    }
    int wd = inotify_add_watch(fd, dir, IN_CLOSE_WRITE | IN_MOVED_TO);
    if (wd == -1) {
        die("inotify_add_watch");
    }
    printf("watching %s for *%s saves\n", dir, suffix);
    fflush(stdout);

    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("read");
        }
        for (char *p = buf; p < buf + n;) {
            const struct inotify_event *ev = (const struct inotify_event *)p;
            size_t name_len = strlen(ev->name);
            size_t suffix_len = strlen(suffix);

            if (ev->len > 0 && name_len >= suffix_len &&
                strcmp(ev->name + name_len - suffix_len, suffix) == 0) {
                char when[16];
                timestamp(when, sizeof when);
                const char *how = (ev->mask & IN_CLOSE_WRITE) ? "written in place"
                                                              : "renamed into place";
                printf("%s  %s  %s\n", when, ev->name, how);
                fflush(stdout);
            }
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
    return 0;
}
