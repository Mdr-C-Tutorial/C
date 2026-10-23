#include "timer.h"

#include <limits.h>
#include <stddef.h>

void timer_queue_init(struct timer_queue *queue, int64_t timeout_ms) {
    // 空队列里哨兵的前驱和后继都指向自己，插入和删除因此不必判断 NULL。
    queue->head.prev = &queue->head;
    queue->head.next = &queue->head;
    queue->head.expires_ms = 0;
    queue->head.owner = NULL;
    queue->timeout_ms = timeout_ms;
}

void timer_init(struct timer *timer, void *owner) {
    timer->prev = NULL;
    timer->next = NULL;
    timer->expires_ms = 0;
    timer->owner = owner;
}

bool timer_is_active(const struct timer *timer) { return timer->next != NULL; }

void timer_cancel(struct timer *timer) {
    if (!timer_is_active(timer)) {
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    timer->prev = NULL;
    timer->next = NULL;
}

void timer_restart(struct timer_queue *queue, struct timer *timer,
                   int64_t now_ms) {
    timer_cancel(timer);
    timer->expires_ms = now_ms + queue->timeout_ms;
    // 插在哨兵之前，也就是队尾。
    struct timer *last = queue->head.prev;
    timer->prev = last;
    timer->next = &queue->head;
    last->next = timer;
    queue->head.prev = timer;
}

static struct timer *first_timer(const struct timer_queue *queue) {
    struct timer *first = queue->head.next;
    return first != &queue->head ? first : NULL;
}

struct timer *timer_queue_pop_expired(struct timer_queue *queue,
                                      int64_t now_ms) {
    struct timer *first = first_timer(queue);
    if (first == NULL || first->expires_ms > now_ms) {
        return NULL;
    }
    timer_cancel(first);
    return first;
}

int timer_queue_wait_ms(const struct timer_queue *queue, int64_t now_ms) {
    const struct timer *first = first_timer(queue);
    if (first == NULL) {
        return -1;
    }
    int64_t wait_ms = first->expires_ms - now_ms;
    if (wait_ms < 0) {
        return 0;
    }
    return wait_ms > INT_MAX ? INT_MAX : (int)wait_ms;
}
