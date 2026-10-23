/* 目录监狱演示：只允许读取 root 目录内的文件。
 * 用法：./jail ROOT PATH
 * 两种模式：realpath 前缀检查（默认）与 openat2 + RESOLVE_BENEATH。
 * 只读打开并把前 64 字节打印出来。 */
#define _GNU_SOURCE /* openat2 没有 glibc 包装，要用 syscall；realpath 需要 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/openat2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static int open_realpath(const char *root, const char *path) {
    char real_root[PATH_MAX];
    char real_path[PATH_MAX];
    size_t n;

    if (realpath(root, real_root) == NULL) {
        perror("realpath root");
        return -1;
    }
    if (realpath(path, real_path) == NULL) {
        perror("realpath path");
        return -1;
    }
    n = strlen(real_root);
    /* 前缀必须到目录边界为止：/srv/pub 不能匹配 /srv/publisher */
    if (strncmp(real_path, real_root, n) != 0 ||
        (real_path[n] != '\0' && real_path[n] != '/')) {
        fprintf(stderr, "realpath: %s escapes root %s\n", path, root);
        errno = EACCES;
        return -1;
    }
    return open(real_path, O_RDONLY | O_CLOEXEC);
}

static int open_beneath(const char *root, const char *path) {
    struct open_how how;
    int dirfd;
    int fd;

    dirfd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd == -1) {
        perror("open root");
        return -1;
    }
    memset(&how, 0, sizeof how);
    how.flags = O_RDONLY | O_CLOEXEC;
    how.resolve = RESOLVE_BENEATH;
    fd = (int)syscall(SYS_openat2, dirfd, path, &how, sizeof how);
    if (fd == -1) {
        fprintf(stderr, "openat2: %s\n", strerror(errno));
    }
    close(dirfd);
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s ROOT PATH [beneath]\n", argv[0]);
        return 1;
    }
    int fd;
    if (argc == 4 && strcmp(argv[3], "beneath") == 0) {
        fd = open_beneath(argv[1], argv[2]);
    } else {
        fd = open_realpath(argv[1], argv[2]);
    }
    if (fd == -1) {
        return 1;
    }
    char buf[64];
    ssize_t n = read(fd, buf, sizeof buf);
    if (n < 0) {
        perror("read");
        close(fd);
        return 1;
    }
    printf("opened, first %zd bytes: ", n);
    fwrite(buf, 1, (size_t)n, stdout);
    if (n == 0 || buf[n - 1] != '\n') {
        printf("\n");
    }
    close(fd);
    return 0;
}