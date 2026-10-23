#ifndef MDR_SERVER_TIMER_H
#define MDR_SERVER_TIMER_H

#include <stdbool.h>
#include <stdint.h>

// 空闲超时定时器队列。
//
// 一般的定时器要按到期时间排序，常用最小堆实现。这里所有定时器的时长
// 都相同（空闲超时），到期时间 = 启动时刻 + 同一个时长，所以“按到期
// 时间排序”就是“按启动的先后排序”：每次（重新）启动都把定时器挂到队尾，
// 队列自然有序，队头总是最早到期的那个。启动、取消、取最早到期的定时器
// 都是 O(1)。
//
// 这个性质只在时长相同时成立，所以时长属于队列，而不属于单个定时器。
//
// 当前时间一律由调用者传入（毫秒，单调时钟），本模块不读时钟，单元测试
// 因此不必真的等待。
struct timer {
    struct timer *prev;
    struct timer *next; // 不在队列中时为 NULL
    int64_t expires_ms;
    void *owner; // 定时器属于哪个对象，到期时由调用者取用
};

struct timer_queue {
    struct timer head; // 哨兵结点：head.next 最早到期，head.prev 最晚到期
    int64_t timeout_ms;
};

void timer_queue_init(struct timer_queue *queue, int64_t timeout_ms);
void timer_init(struct timer *timer, void *owner);
bool timer_is_active(const struct timer *timer);

// 从 now_ms 起重新计时：到期时间设为 now_ms + timeout_ms，挂到队尾。
void timer_restart(struct timer_queue *queue, struct timer *timer,
                   int64_t now_ms);

// 从队列中摘下定时器；对不在队列中的定时器调用也没有关系。
void timer_cancel(struct timer *timer);

// 摘下并返回一个已到期（expires_ms <= now_ms）的定时器，没有时返回 NULL。
struct timer *timer_queue_pop_expired(struct timer_queue *queue,
                                      int64_t now_ms);

// 距离最早的到期时间还有多少毫秒，可直接用作 epoll_wait 的超时参数；
// 队列为空时返回 -1，表示无限等待。
int timer_queue_wait_ms(const struct timer_queue *queue, int64_t now_ms);

#endif
