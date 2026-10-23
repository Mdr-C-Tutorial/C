#ifndef EVL_H
#define EVL_H

#include <stdint.h>

/*
 * evl：一个最小的 reactor 事件循环。
 * 文件事件基于 epoll（水平触发），定时器基于单调时钟，另有延迟任务队列。
 */

typedef struct evl_loop evl_loop;
typedef struct evl_timer evl_timer;

enum {
    EVL_READ = 1u << 0,
    EVL_WRITE = 1u << 1,
};

/* 文件事件处理函数：fd 就绪时调用，events 是 EVL_READ 与 EVL_WRITE 的组合。 */
typedef void (*evl_file_cb)(evl_loop *loop, int fd, unsigned events, void *arg);
/* 定时器处理函数：到期时调用，回调里可以取消自己。 */
typedef void (*evl_timer_cb)(evl_loop *loop, evl_timer *timer, void *arg);
/* 延迟任务：本轮事件全部处理完后调用。 */
typedef void (*evl_defer_cb)(evl_loop *loop, void *arg);

evl_loop *evl_create(void);
void evl_destroy(evl_loop *loop);

int evl_add_fd(evl_loop *loop, int fd, unsigned mask, evl_file_cb cb, void *arg);
int evl_mod_fd(evl_loop *loop, int fd, unsigned mask);
int evl_del_fd(evl_loop *loop, int fd);

evl_timer *evl_add_timer(evl_loop *loop, int64_t delay_ms, int64_t repeat_ms,
                         evl_timer_cb cb, void *arg);
void evl_cancel_timer(evl_loop *loop, evl_timer *timer);

int evl_defer(evl_loop *loop, evl_defer_cb cb, void *arg);
void evl_stop(evl_loop *loop);
int evl_run(evl_loop *loop);

int64_t evl_now_ms(void);

#endif