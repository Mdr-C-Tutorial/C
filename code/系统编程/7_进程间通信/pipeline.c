#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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

    pid_t left = fork();
    if (left == -1) {
        die("fork");
    }
    if (left == 0) {
        /* ls -l：标准输出接到管道写端 */
        if (dup2(fds[1], STDOUT_FILENO) == -1) {
            die("dup2");
        }
        close(fds[0]);
        close(fds[1]);
        execlp("ls", "ls", "-l", (char *)NULL);
        die("execlp ls");
    }

    pid_t right = fork();
    if (right == -1) {
        die("fork");
    }
    if (right == 0) {
        /* wc -l：标准输入接在管道读端 */
        if (dup2(fds[0], STDIN_FILENO) == -1) {
            die("dup2");
        }
        close(fds[0]);
        close(fds[1]);
        execlp("wc", "wc", "-l", (char *)NULL);
        die("execlp wc");
    }

    /* 父进程两个方向都不需要，全关掉，否则 wc 等不到文件结束 */
    close(fds[0]);
    close(fds[1]);
    if (waitpid(left, NULL, 0) == -1) {
        die("waitpid");
    }
    if (waitpid(right, NULL, 0) == -1) {
        die("waitpid");
    }
    return EXIT_SUCCESS;
}
