#define _POSIX_C_SOURCE 200809L

#include "evl.h"

#include <signal.h>
#include <stdio.h>
#include <sys/signalfd.h>
#include <unistd.h>

static int ticks;

static void on_tick(evl_loop *loop, evl_timer *timer, void *arg) {
    (void)timer;
    (void)loop;
    (void)arg;
    ticks++;
    printf("心跳 %d\n", ticks);
}

static void on_signal(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct signalfd_siginfo info;

    (void)events;
    (void)arg;
    while (read(fd, &info, sizeof info) == sizeof info) {
        printf("从 signalfd 读到信号 %d\n", info.ssi_signo);
        if (info.ssi_signo == SIGTERM) {
            printf("收到 SIGTERM，退出\n");
            evl_stop(loop);
        }
    }
}

int main(void) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        perror("sigprocmask");
        return 1;
    }
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);

    if (sfd < 0) {
        perror("signalfd");
        return 1;
    }
    evl_loop *loop = evl_create();

    if (evl_add_fd(loop, sfd, EVL_READ, on_signal, NULL) < 0) {
        perror("evl_add_fd");
        return 1;
    }
    evl_add_timer(loop, 300, 300, on_tick, NULL);
    printf("pid = %ld，等待信号\n", (long)getpid());
    fflush(stdout);
    evl_run(loop);
    evl_destroy(loop);
    close(sfd);
    return 0;
}
