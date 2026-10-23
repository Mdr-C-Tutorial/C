#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 写入的数据读完之后，只要还有写端开着，read 就阻塞；
   所有写端关闭之后 read 才返回 0。 */
int main(void) {
    int fds[2];
    if (pipe(fds) == -1) {
        die("pipe");
    }
    if (write(fds[1], "abc", 3) != 3) {
        die("write");
    }

    pid_t pid = fork();
    if (pid == -1) {
        die("fork");
    }
    if (pid == 0) {
        close(fds[1]);
        char buf[16];
        ssize_t n = read(fds[0], buf, sizeof buf);
        printf("子进程第一次 read：%zd 字节\n", n);
        fflush(stdout);
        n = read(fds[0], buf, sizeof buf);
        printf("子进程第二次 read：%zd 字节\n", n);
        fflush(stdout);
        _exit(EXIT_SUCCESS);
    }

    sleep(1); /* 父进程还拿着写端，子进程此时阻塞在第二次 read */
    printf("父进程睡了 1 秒，现在关闭写端\n");
    fflush(stdout);
    close(fds[1]);
    if (waitpid(pid, NULL, 0) == -1) {
        die("waitpid");
    }
    close(fds[0]);
    return EXIT_SUCCESS;
}