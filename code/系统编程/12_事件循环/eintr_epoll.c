#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

static volatile sig_atomic_t got;

static void on_alarm(int signo) {
    (void)signo;
    got = 1;
}

int main(int argc, char *argv[]) {
    struct sigaction sa = {0};
    int use_restart = argc > 1 && strcmp(argv[1], "--restart") == 0;

    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = use_restart ? SA_RESTART : 0;
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    int epfd = epoll_create1(EPOLL_CLOEXEC);

    if (epfd < 0) {
        perror("epoll_create1");
        return 1;
    }
    struct epoll_event events[4];

    alarm(1);
    int n = epoll_wait(epfd, events, 4, -1);
    printf("SA_RESTART %s：epoll_wait 返回 %d，errno = %d (%s)，got = %d\n",
           use_restart ? "已设置" : "未设置", n, errno, strerror(errno), (int)got);
    return 0;
}