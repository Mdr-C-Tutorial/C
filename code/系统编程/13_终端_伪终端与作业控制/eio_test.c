#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    int master = posix_openpt(O_RDWR | O_NOCTTY);

    grantpt(master);
    unlockpt(master);
    char *name = ptsname(master);
    pid_t pid = fork();

    if (pid == 0) {
        setsid();
        int slave = open(name, O_RDWR);
        dup2(slave, 0); dup2(slave, 1); dup2(slave, 2);
        close(slave);
        close(master);
        execlp("sh", "sh", "-c", "echo from child; exit 3", (char *)NULL);
        _exit(127);
    }
    char buf[256];
    ssize_t n;
    while ((n = read(master, buf, sizeof buf)) > 0) {
        if (write(1, buf, (size_t)n) < 0) break;
    }
    printf("read returned %zd, errno=%d (%s)\n", n, errno, strerror(errno));
    int status;
    waitpid(pid, &status, 0);
    printf("child exited with status %d\n", WEXITSTATUS(status));
    n = write(master, "x", 1);
    printf("write to master after child exit: %zd, errno=%d (%s)\n", n, errno, strerror(errno));
    return 0;
}
