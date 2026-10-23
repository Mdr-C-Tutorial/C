#define _POSIX_C_SOURCE 200809L

#include "evl.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static int pipe_fd[2];

static void on_term(int signo) {
    int saved = errno;
    unsigned char byte = (unsigned char)signo;

    if (write(pipe_fd[1], &byte, 1) < 0) {
        /* 管道满时丢弃，说明已有未处理的信号 */
    }
    errno = saved;
}

static void on_pipe(evl_loop *loop, int fd, unsigned events, void *arg) {
    unsigned char byte;

    (void)events;
    (void)arg;
    while (read(fd, &byte, sizeof byte) == sizeof byte) {
        printf("从管道收到信号 %u，在循环线程里安全地处理\n", (unsigned)byte);
        evl_stop(loop);
    }
}

int main(void) {
    struct sigaction sa = {0};

    if (pipe(pipe_fd) < 0) {
        perror("pipe");
        return 1;
    }
    for (int i = 0; i < 2; i++) {
        int flags = fcntl(pipe_fd[i], F_GETFL);

        if (flags < 0 || fcntl(pipe_fd[i], F_SETFL, flags | O_NONBLOCK) < 0) {
            perror("fcntl");
            return 1;
        }
    }
    sa.sa_handler = on_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    evl_loop *loop = evl_create();

    if (evl_add_fd(loop, pipe_fd[0], EVL_READ, on_pipe, NULL) < 0) {
        perror("evl_add_fd");
        return 1;
    }
    printf("pid = %ld\n", (long)getpid());
    fflush(stdout);
    evl_run(loop);
    evl_destroy(loop);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return 0;
}
