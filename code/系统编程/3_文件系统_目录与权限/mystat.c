#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static const char *file_type(mode_t mode) {
    if (S_ISREG(mode)) {
        return "regular file";
    }
    if (S_ISDIR(mode)) {
        return "directory";
    }
    if (S_ISLNK(mode)) {
        return "symbolic link";
    }
    if (S_ISFIFO(mode)) {
        return "FIFO";
    }
    if (S_ISSOCK(mode)) {
        return "socket";
    }
    if (S_ISCHR(mode)) {
        return "character device";
    }
    if (S_ISBLK(mode)) {
        return "block device";
    }
    return "unknown";
}

static char type_char(mode_t mode) {
    if (S_ISREG(mode)) {
        return '-';
    }
    if (S_ISDIR(mode)) {
        return 'd';
    }
    if (S_ISLNK(mode)) {
        return 'l';
    }
    if (S_ISFIFO(mode)) {
        return 'p';
    }
    if (S_ISSOCK(mode)) {
        return 's';
    }
    if (S_ISCHR(mode)) {
        return 'c';
    }
    if (S_ISBLK(mode)) {
        return 'b';
    }
    return '?';
}

/* 生成 ls -l 那样的权限串；s、t 占用 x 的位置，大写表示 x 位为 0。 */
static void format_mode(mode_t mode, char out[11]) {
    static const char rwx[] = "rwx";

    out[0] = type_char(mode);
    for (int i = 0; i < 9; i++) {
        out[i + 1] = (mode & (0400u >> i)) ? rwx[i % 3] : '-';
    }
    if (mode & 04000) {
        out[3] = (out[3] == 'x') ? 's' : 'S';
    }
    if (mode & 02000) {
        out[6] = (out[6] == 'x') ? 's' : 'S';
    }
    if (mode & 01000) {
        out[9] = (out[9] == 'x') ? 't' : 'T';
    }
    out[10] = '\0';
}

static void print_time(const char *label, struct timespec ts) {
    char buf[64];
    struct tm tm;

    if (localtime_r(&ts.tv_sec, &tm) == NULL) {
        die("localtime_r");
    }
    if (strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S %z", &tm) == 0) {
        fprintf(stderr, "strftime: buffer too small\n");
        exit(EXIT_FAILURE);
    }
    printf("%-7s %s.%09ld\n", label, buf, ts.tv_nsec);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s PATH...\n", argv[0]);
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i++) {
        struct stat st;
        char mode[11];

        if (lstat(argv[i], &st) == -1) {
            perror(argv[i]);
            continue;
        }
        format_mode(st.st_mode, mode);
        printf("  File: %s\n", argv[i]);
        printf("  Type: %s\n", file_type(st.st_mode));
        printf("  Mode: %s (%04o)  Links: %llu  Inode: %llu\n", mode,
               (unsigned)(st.st_mode & 07777),
               (unsigned long long)st.st_nlink,
               (unsigned long long)st.st_ino);
        printf("Device: %u,%u  Size: %lld  Blocks: %lld  Blksize: %lld\n",
               major(st.st_dev), minor(st.st_dev), (long long)st.st_size,
               (long long)st.st_blocks, (long long)st.st_blksize);
        print_time("Access:", st.st_atim);
        print_time("Modify:", st.st_mtim);
        print_time("Change:", st.st_ctim);
        if (i + 1 < argc) {
            printf("\n");
        }
    }
    return 0;
}
