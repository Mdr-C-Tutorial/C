#define _POSIX_C_SOURCE 200809L

#include "evl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct conn {
    int fd;
    char name;
    evl_timer *idle_timer;
};

static void on_idle(evl_loop *loop, evl_timer *timer, void *arg) {
    struct conn *c = arg;

    (void)loop;
    printf("连接 %c 的空闲定时器触发，访问 c->name\n", c->name);
    (void)timer;
}

static void on_readable(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct conn *c = arg;
    char buf[16];

    (void)events;
    if (read(fd, buf, sizeof buf) < 0) {
        perror("read");
    }
    printf("连接 %c 收到数据，关闭并释放自己\n", c->name);
    evl_del_fd(loop, fd);
    close(fd);
    free(c);
}

static void stop_all(evl_loop *loop, evl_timer *timer, void *arg) {
    (void)timer;
    (void)arg;
    evl_stop(loop);
}

int main(void) {
    evl_loop *loop = evl_create();
    int p[2];
    struct conn *c;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (pipe(p) < 0) {
        perror("pipe");
        return 1;
    }
    c = malloc(sizeof *c);
    c->fd = p[0];
    c->name = 'A';
    c->idle_timer = evl_add_timer(loop, 0, 0, on_idle, c);
    if (evl_add_fd(loop, p[0], EVL_READ, on_readable, c) < 0) {
        perror("evl_add_fd");
        return 1;
    }
    if (write(p[1], "x", 1) != 1) {
        perror("write");
        return 1;
    }
    evl_add_timer(loop, 100, 0, stop_all, NULL);
    evl_run(loop);
    evl_destroy(loop);
    return 0;
}
