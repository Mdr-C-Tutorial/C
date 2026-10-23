#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void flush_stdout(void) {
    if (fflush(stdout) == EOF) {
        die("fflush");
    }
}

int main(void) {
    printf("第 1 行：重定向之前\n");
    flush_stdout(); /* 缓冲区里的内容先写到原来的标准输出 */

    int saved = fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 0); /* 留一个副本，以便恢复 */
    if (saved == -1) {
        die("fcntl");
    }
    int fd = open("out.log", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd == -1) {
        die("open");
    }
    if (dup2(fd, STDOUT_FILENO) == -1) {
        die("dup2");
    }
    if (close(fd) == -1) {
        die("close");
    }

    printf("第 2 行：写进 out.log\n");
    flush_stdout();

    if (dup2(saved, STDOUT_FILENO) == -1) { /* 恢复原来的标准输出 */
        die("dup2");
    }
    if (close(saved) == -1) {
        die("close");
    }
    printf("第 3 行：恢复之后\n");
    return 0;
}