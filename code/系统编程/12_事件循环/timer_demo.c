#define _POSIX_C_SOURCE 200809L

#include "evl.h"

#include <stdio.h>

static int64_t start_ms;

static void stamp(const char *what) {
    printf("[%5lld ms] %s\n", (long long)(evl_now_ms() - start_ms), what);
}

static void on_tick(evl_loop *loop, evl_timer *timer, void *arg) {
    (void)timer;
    (void)arg;
    static int count = 0;

    count++;
    stamp("tick");
    if (count == 3) {
        evl_cancel_timer(loop, timer);
        stamp("tick 取消自己");
    }
}

static void on_once(evl_loop *loop, evl_timer *timer, void *arg) {
    (void)timer;
    stamp((const char *)arg);
    evl_stop(loop);
}

static void on_defer(evl_loop *loop, void *arg) {
    (void)loop;
    stamp((const char *)arg);
}

int main(void) {
    evl_loop *loop = evl_create();

    start_ms = evl_now_ms();
    evl_defer(loop, on_defer, "defer：本轮分派结束后执行");
    if (evl_add_timer(loop, 120, 100, on_tick, NULL) == NULL) {
        return 1;
    }
    if (evl_add_timer(loop, 450, 0, on_once, "一次性定时器 450 ms") == NULL) {
        return 1;
    }
    stamp("进入循环");
    evl_run(loop);
    stamp("循环结束");
    evl_destroy(loop);
    return 0;
}
