#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void show(const char *name, int fd) {
    int fdfl = fcntl(fd, F_GETFD);
    if (fdfl == -1) {
        die("fcntl");
    }
    printf("%s：描述符 %d，FD_CLOEXEC %s\n", name, fd,
           (fdfl & FD_CLOEXEC) ? "已设置" : "未设置");
}

int main(void) {
    int plain = open("plain.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int clo = open("clo.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (plain == -1 || clo == -1) {
        die("open");
    }
    show("不带 O_CLOEXEC", plain);
    show("带 O_CLOEXEC", clo);

    if (fcntl(plain, F_SETFD, FD_CLOEXEC) == -1) {
        die("fcntl");
    }
    show("给第一个补上之后", plain);
    close(plain);
    close(clo);
    return 0;
}