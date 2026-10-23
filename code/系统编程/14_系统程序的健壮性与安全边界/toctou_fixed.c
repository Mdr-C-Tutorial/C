/* TOCTOU 的修正写法：不做单独的检查，直接打开并检查结果；
 * 需要禁止跟随符号链接时加 O_NOFOLLOW；相对可信目录打开时用 openat。 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s DIR NAME\n", argv[0]);
        return 1;
    }
    int dirfd = open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd == -1) {
        perror("open dir");
        return 1;
    }
    int fd = openat(dirfd, argv[2], O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC);
    if (fd == -1) {
        fprintf(stderr, "openat: %s\n", strerror(errno));
        return 1;
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "not a regular file\n");
        return 1;
    }
    static const char msg[] = "appended safely\n";
    if (write(fd, msg, sizeof msg - 1) == -1) {
        perror("write");
        return 1;
    }
    close(fd);
    close(dirfd);
    printf("appended to %s without a separate check\n", argv[2]);
    return 0;
}