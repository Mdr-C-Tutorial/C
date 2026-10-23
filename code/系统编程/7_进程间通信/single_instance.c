#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

#define LOCK_FILE "run.lock"

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int hold = argc > 1 ? atoi(argv[1]) : 3;

    /* 后台运行时标准输出是管道，行缓冲改成每次都立刻写出，
       这样两个实例的输出顺序和实际发生的一致 */
    setvbuf(stdout, NULL, _IOLBF, 0);

    int fd = open(LOCK_FILE, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd == -1) {
        die("open");
    }
    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        printf("pid %d：已在运行，退出\n", (int)getpid());
        return 1;
    }

    printf("pid %d：拿到锁，工作 %d 秒\n", (int)getpid(), hold);
    struct timespec ts = {hold, 0};
    nanosleep(&ts, NULL);
    printf("pid %d：结束，锁随 close 释放\n", (int)getpid());
    close(fd);
    return 0;
}
