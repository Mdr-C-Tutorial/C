#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 把 timespec 的秒部分格式化成 HH:MM:SS。 */
static void fmt_time(const struct timespec *ts, char out[16]) {
    struct tm tm;

    if (localtime_r(&ts->tv_sec, &tm) == NULL) {
        die("localtime_r");
    }
    if (strftime(out, 16, "%H:%M:%S", &tm) == 0) {
        fprintf(stderr, "strftime: buffer too small\n");
        exit(EXIT_FAILURE);
    }
}

static void show(const char *tag, const char *path) {
    struct stat st;
    char a[16];
    char m[16];
    char c[16];

    if (stat(path, &st) == -1) {
        die("stat");
    }
    fmt_time(&st.st_atim, a);
    fmt_time(&st.st_mtim, m);
    fmt_time(&st.st_ctim, c);
    printf("%-10s atime=%s.%03ld  mtime=%s.%03ld  ctime=%s.%03ld\n", tag, a,
           st.st_atim.tv_nsec / 1000000, m, st.st_mtim.tv_nsec / 1000000, c,
           st.st_ctim.tv_nsec / 1000000);
}

int main(void) {
    const char *path = "stamp.txt";

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd == -1) {
        die("open");
    }
    if (write(fd, "hello\n", 6) == -1) {
        die("write");
    }
    if (close(fd) == -1) {
        die("close");
    }
    show("create", path);
    sleep(1);

    char buf[16];
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        die("open");
    }
    if (read(fd, buf, sizeof buf) == -1) {
        die("read");
    }
    if (close(fd) == -1) {
        die("close");
    }
    show("read", path);
    sleep(1);

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        die("open");
    }
    if (read(fd, buf, sizeof buf) == -1) {
        die("read");
    }
    if (close(fd) == -1) {
        die("close");
    }
    show("read again", path);
    sleep(1);

    fd = open(path, O_WRONLY | O_APPEND | O_CLOEXEC);
    if (fd == -1) {
        die("open");
    }
    if (write(fd, "again\n", 6) == -1) {
        die("write");
    }
    if (close(fd) == -1) {
        die("close");
    }
    show("write", path);
    sleep(1);

    if (chmod(path, 0600) == -1) {
        die("chmod");
    }
    show("chmod", path);
    sleep(1);

    struct timespec times[2] = {
        { .tv_sec = 1000000000, .tv_nsec = 0 },
        { .tv_sec = 1000000001, .tv_nsec = 0 },
    };
    if (utimensat(AT_FDCWD, path, times, 0) == -1) {
        die("utimensat");
    }
    show("utimensat", path);
    return 0;
}
