#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* 父进程分别以带与不带 O_CLOEXEC 的方式打开同一个文件，
 * 然后 exec fdlist 查看子进程继承到的描述符。 */
int main(void) {
    int no_cloexec = open("data.txt", O_RDONLY);
    int with_cloexec = open("data.txt", O_RDONLY | O_CLOEXEC);
    if (no_cloexec == -1 || with_cloexec == -1) {
        perror("open");
        return EXIT_FAILURE;
    }
    printf("parent: fd %d without O_CLOEXEC, fd %d with O_CLOEXEC\n",
           no_cloexec, with_cloexec);
    fflush(stdout);
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        execl("./fdlist", "fdlist", (char *)NULL);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    printf("child exit=%d\n", WEXITSTATUS(status));
    return 0;
}