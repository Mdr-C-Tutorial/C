#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static const struct {
    uint32_t mask;
    const char *name;
} event_names[] = {
    { IN_ACCESS, "IN_ACCESS" },
    { IN_ATTRIB, "IN_ATTRIB" },
    { IN_CLOSE_WRITE, "IN_CLOSE_WRITE" },
    { IN_CLOSE_NOWRITE, "IN_CLOSE_NOWRITE" },
    { IN_CREATE, "IN_CREATE" },
    { IN_DELETE, "IN_DELETE" },
    { IN_DELETE_SELF, "IN_DELETE_SELF" },
    { IN_MODIFY, "IN_MODIFY" },
    { IN_MOVE_SELF, "IN_MOVE_SELF" },
    { IN_MOVED_FROM, "IN_MOVED_FROM" },
    { IN_MOVED_TO, "IN_MOVED_TO" },
    { IN_OPEN, "IN_OPEN" },
};

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : ".";

    int fd = inotify_init1(IN_CLOEXEC);
    if (fd == -1) {
        die("inotify_init1");
    }
    int wd = inotify_add_watch(fd, path, IN_ALL_EVENTS);
    if (wd == -1) {
        die("inotify_add_watch");
    }
    printf("watching %s (wd=%d), press Ctrl+C to stop\n", path, wd);
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

            printf("wd=%d cookie=%u ", ev->wd, ev->cookie);
            const char *sep = "";
            for (size_t i = 0; i < sizeof event_names / sizeof event_names[0];
                 i++) {
                if (ev->mask & event_names[i].mask) {
                    printf("%s%s", sep, event_names[i].name);
                    sep = "|";
                }
            }
            if (ev->mask & IN_ISDIR) {
                printf("|IN_ISDIR");
            }
            if (ev->len > 0) {
                printf(" %s", ev->name);
            }
            printf("\n");
            fflush(stdout);
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
    return 0;
}
