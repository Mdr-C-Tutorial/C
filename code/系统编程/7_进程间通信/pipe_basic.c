#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    int fds[2];
    if (pipe(fds) == -1) {
        die("pipe");
    }

    pid_t pid = fork();
    if (pid == -1) {
        die("fork");
    }
    if (pid == 0) {
        close(fds[1]); /* 子进程只读，写端关掉 */
        char buf[256];
        ssize_t n = read(fds[0], buf, sizeof buf - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("子进程收到：%s", buf);
            fflush(stdout); /* 子进程用 _exit 退出，缓冲区要自己刷 */
        }
        close(fds[0]);
        _exit(EXIT_SUCCESS);
    }

    close(fds[0]); /* 父进程只写，读端关掉 */
    const char *msg = "hello from parent\n";
    if (write(fds[1], msg, strlen(msg)) != (ssize_t)strlen(msg)) {
        die("write");
    }
    close(fds[1]);
    if (waitpid(pid, NULL, 0) == -1) {
        die("waitpid");
    }
    return 0;
}