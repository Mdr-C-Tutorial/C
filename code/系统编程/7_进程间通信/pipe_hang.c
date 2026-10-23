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

int main(int argc, char *argv[]) {
    int fix = argc > 1 && strcmp(argv[1], "fix") == 0;

    int fds[2];
    if (pipe(fds) == -1) {
        die("pipe");
    }

    pid_t pid = fork();
    if (pid == -1) {
        die("fork");
    }

    if (pid == 0) {
        if (fix) {
            /* 修正版：子进程不需要写端，先把它关掉 */
            if (close(fds[1]) == -1) {
                die("close");
            }
        }
        char buf[256];
        for (;;) {
            ssize_t n = read(fds[0], buf, sizeof buf - 1);
            if (n == -1) {
                die("read");
            }
            if (n == 0) {
                puts("子进程：读到文件结束");
                break;
            }
            buf[n] = '\0';
            printf("子进程收到：%s", buf);
            fflush(stdout);
        }
        _exit(EXIT_SUCCESS);
    }

    if (close(fds[0]) == -1) {
        die("close");
    }
    const char *msg = "hello from parent\n";
    if (write(fds[1], msg, strlen(msg)) != (ssize_t)strlen(msg)) {
        die("write");
    }
    if (close(fds[1]) == -1) {
        die("close");
    }
    if (waitpid(pid, NULL, 0) == -1) {
        die("waitpid");
    }
    puts("父进程：子进程已退出");
    return EXIT_SUCCESS;
}
