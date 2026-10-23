#define _POSIX_C_SOURCE 200809L

#include "evl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct conn {
    int fd;
    char name;
    struct conn *other;
    int closing;
};

static void conn_free(evl_loop *loop, void *arg) {
    struct conn *c = arg;

    (void)loop;
    printf("本轮结束，释放连接 %c 的结构\n", c->name);
    free(c);
}

static void conn_close(evl_loop *loop, struct conn *c) {
    if (c->closing) {
        return;
    }
    c->closing = 1;
    evl_del_fd(loop, c->fd);
    close(c->fd);
    evl_defer(loop, conn_free, c);
}

static void on_readable(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct conn *c = arg;
    char buf[16];

    (void)events;
    if (c->closing) {
        return;
    }
    if (read(fd, buf, sizeof buf) < 0) {
        perror("read");
    }
    printf("连接 %c 的处理器运行\n", c->name);
    if (c->other != NULL && !c->other->closing) {
        printf("连接 %c 关闭连接 %c（只标记，稍后释放）\n", c->name, c->other->name);
        conn_close(loop, c->other);
        c->other = NULL;
    }
    printf("连接 %c 处理完毕\n", c->name);
}

static void on_timeout(evl_loop *loop, evl_timer *timer, void *arg) {
    (void)timer;
    (void)arg;
    evl_stop(loop);
}

int main(void) {
    evl_loop *loop = evl_create();
    int pa[2];
    int pb[2];
    struct conn *a;
    struct conn *b;

    if (pipe(pa) < 0 || pipe(pb) < 0) {
        perror("pipe");
        return 1;
    }
    a = calloc(1, sizeof *a);
    b = calloc(1, sizeof *b);
    a->fd = pa[0];
    a->name = 'A';
    a->other = b;
    b->fd = pb[0];
    b->name = 'B';
    b->other = a;

    if (evl_add_fd(loop, pa[0], EVL_READ, on_readable, a) < 0 ||
        evl_add_fd(loop, pb[0], EVL_READ, on_readable, b) < 0) {
        perror("evl_add_fd");
        return 1;
    }
    if (write(pa[1], "x", 1) != 1 || write(pb[1], "y", 1) != 1) {
        perror("write");
        return 1;
    }
    evl_add_timer(loop, 100, 0, on_timeout, NULL);
    evl_run(loop);
    evl_destroy(loop);
    close(pa[1]);
    close(pb[1]);
    return 0;
}
