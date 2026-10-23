#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    int fd = memfd_create("p087_counter", MFD_CLOEXEC);
    if (fd == -1) {
        die("memfd_create");
    }
    if (ftruncate(fd, 4096) == -1) {
        die("ftruncate");
    }
    long *counter = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (counter == MAP_FAILED) {
        die("mmap");
    }
    *counter = 0;

    printf("memfd 的链接目标：");
    char link[64];
    snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    char target[256];
    ssize_t n = readlink(link, target, sizeof target - 1);
    if (n == -1) {
        die("readlink");
    }
    target[n] = '\0';
    printf("%s\n", target);

    pid_t pid = fork();
    if (pid == -1) {
        die("fork");
    }
    if (pid == 0) {
        for (int i = 0; i < 1000; i++) {
            *counter += 1;
        }
        _exit(EXIT_SUCCESS);
    }
    if (waitpid(pid, NULL, 0) == -1) {
        die("waitpid");
    }
    printf("子进程加完，计数器 = %ld\n", *counter);

    if (ftruncate(fd, 8192) == -1) {
        die("ftruncate 扩大");
    }
    printf("扩大后文件大小 = %ld\n", (long)lseek(fd, 0, SEEK_END));

    munmap(counter, 4096);
    close(fd);
    return EXIT_SUCCESS;
}
