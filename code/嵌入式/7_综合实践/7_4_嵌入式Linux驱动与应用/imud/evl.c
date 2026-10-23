#define _POSIX_C_SOURCE 200809L

/*
 * evl：一个最小的 reactor 事件循环。
 *
 * 所有权规则的约定：
 *   - 循环不关闭任何描述符，调用方在 evl_del_fd 之后自己 close；
 *   - evl_add_fd 要求同一个描述符只登记一次；
 *   - 定时器由 evl 分配和释放，evl_cancel_timer 之后句柄即失效；
 *   - 循环和回调都在同一个线程里运行，别的线程只能通过 eventfd 之类
 *     的描述符把工作交回来。
 */

#include "evl.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#define EVL_MAX_EVENTS 64
#define EVL_NO_INDEX ((size_t)-1)

struct evl_fd {
    int fd;
    unsigned mask;
    evl_file_cb cb;
    void *arg;
};

struct evl_timer {
    int64_t when_ms;
    int64_t repeat_ms;
    evl_timer_cb cb;
    void *arg;
    size_t heap_index;
    struct evl_timer *prev;
    struct evl_timer *next;
};

struct evl_deferred {
    evl_defer_cb cb;
    void *arg;
    struct evl_deferred *next;
};

struct evl_loop {
    int epfd;
    struct evl_fd **fds;
    int fd_cap;
    struct epoll_event events[EVL_MAX_EVENTS];
    struct evl_timer **heap;
    size_t heap_len;
    size_t heap_cap;
    struct evl_timer *timers;
    struct evl_deferred *defer_head;
    struct evl_deferred *defer_tail;
    int stop;
};

int64_t evl_now_ms(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint32_t to_epoll_mask(unsigned mask) {
    uint32_t events = 0;

    if (mask & EVL_READ) {
        events |= EPOLLIN;
    }
    if (mask & EVL_WRITE) {
        events |= EPOLLOUT;
    }
    return events;
}

evl_loop *evl_create(void) {
    evl_loop *loop = calloc(1, sizeof *loop);

    if (loop == NULL) {
        return NULL;
    }
    loop->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epfd < 0) {
        free(loop);
        return NULL;
    }
    return loop;
}

static void free_timers(evl_loop *loop) {
    evl_timer *timer = loop->timers;

    while (timer != NULL) {
        evl_timer *next = timer->next;

        free(timer);
        timer = next;
    }
    loop->timers = NULL;
}

void evl_destroy(evl_loop *loop) {
    if (loop == NULL) {
        return;
    }
    free_timers(loop);
    while (loop->defer_head != NULL) {
        struct evl_deferred *next = loop->defer_head->next;

        free(loop->defer_head);
        loop->defer_head = next;
    }
    for (int fd = 0; fd < loop->fd_cap; fd++) {
        free(loop->fds[fd]);
    }
    free(loop->fds);
    free(loop->heap);
    close(loop->epfd);
    free(loop);
}

static int grow_fd_table(evl_loop *loop, int fd) {
    int cap = loop->fd_cap > 0 ? loop->fd_cap : 16;

    while (cap <= fd) {
        cap *= 2;
    }
    struct evl_fd **fds = realloc(loop->fds, (size_t)cap * sizeof *fds);

    if (fds == NULL) {
        return -1;
    }
    memset(fds + loop->fd_cap, 0, (size_t)(cap - loop->fd_cap) * sizeof *fds);
    loop->fds = fds;
    loop->fd_cap = cap;
    return 0;
}

int evl_add_fd(evl_loop *loop, int fd, unsigned mask, evl_file_cb cb, void *arg) {
    if (fd < 0 || cb == NULL || mask == 0 || (mask & ~(EVL_READ | EVL_WRITE)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (fd >= loop->fd_cap && grow_fd_table(loop, fd) < 0) {
        return -1;
    }
    if (loop->fds[fd] != NULL) {
        errno = EEXIST;
        return -1;
    }
    struct evl_fd *entry = malloc(sizeof *entry);

    if (entry == NULL) {
        return -1;
    }
    entry->fd = fd;
    entry->mask = mask;
    entry->cb = cb;
    entry->arg = arg;

    struct epoll_event ev = {
        .events = to_epoll_mask(mask),
        .data.fd = fd,
    };

    if (epoll_ctl(loop->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        int saved = errno;

        free(entry);
        errno = saved;
        return -1;
    }
    loop->fds[fd] = entry;
    return 0;
}

int evl_mod_fd(evl_loop *loop, int fd, unsigned mask) {
    if (fd < 0 || fd >= loop->fd_cap || loop->fds[fd] == NULL || mask == 0 ||
        (mask & ~(EVL_READ | EVL_WRITE)) != 0) {
        errno = EINVAL;
        return -1;
    }
    struct epoll_event ev = {
        .events = to_epoll_mask(mask),
        .data.fd = fd,
    };

    if (epoll_ctl(loop->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        return -1;
    }
    loop->fds[fd]->mask = mask;
    return 0;
}

int evl_del_fd(evl_loop *loop, int fd) {
    if (fd < 0 || fd >= loop->fd_cap || loop->fds[fd] == NULL) {
        errno = ENOENT;
        return -1;
    }
    if (epoll_ctl(loop->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        return -1;
    }
    free(loop->fds[fd]);
    loop->fds[fd] = NULL;
    return 0;
}

static void heap_swap(evl_loop *loop, size_t lhs, size_t rhs) {
    evl_timer *tmp = loop->heap[lhs];

    loop->heap[lhs] = loop->heap[rhs];
    loop->heap[rhs] = tmp;
    loop->heap[lhs]->heap_index = lhs;
    loop->heap[rhs]->heap_index = rhs;
}

static void heap_sift_up(evl_loop *loop, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;

        if (loop->heap[parent]->when_ms <= loop->heap[index]->when_ms) {
            break;
        }
        heap_swap(loop, parent, index);
        index = parent;
    }
}

static void heap_sift_down(evl_loop *loop, size_t index) {
    for (;;) {
        size_t left = index * 2 + 1;
        size_t right = left + 1;
        size_t smallest = index;

        if (left < loop->heap_len &&
            loop->heap[left]->when_ms < loop->heap[smallest]->when_ms) {
            smallest = left;
        }
        if (right < loop->heap_len &&
            loop->heap[right]->when_ms < loop->heap[smallest]->when_ms) {
            smallest = right;
        }
        if (smallest == index) {
            break;
        }
        heap_swap(loop, index, smallest);
        index = smallest;
    }
}

static int heap_push(evl_loop *loop, evl_timer *timer) {
    if (loop->heap_len == loop->heap_cap) {
        size_t cap = loop->heap_cap > 0 ? loop->heap_cap * 2 : 16;
        evl_timer **heap = realloc(loop->heap, cap * sizeof *heap);

        if (heap == NULL) {
            return -1;
        }
        loop->heap = heap;
        loop->heap_cap = cap;
    }
    loop->heap[loop->heap_len] = timer;
    timer->heap_index = loop->heap_len;
    loop->heap_len++;
    heap_sift_up(loop, timer->heap_index);
    return 0;
}

static evl_timer *heap_pop(evl_loop *loop) {
    evl_timer *top = loop->heap[0];

    loop->heap_len--;
    if (loop->heap_len > 0) {
        loop->heap[0] = loop->heap[loop->heap_len];
        loop->heap[0]->heap_index = 0;
        heap_sift_down(loop, 0);
    }
    top->heap_index = EVL_NO_INDEX;
    return top;
}

static void heap_remove(evl_loop *loop, evl_timer *timer) {
    size_t index = timer->heap_index;

    loop->heap_len--;
    if (index != loop->heap_len) {
        loop->heap[index] = loop->heap[loop->heap_len];
        loop->heap[index]->heap_index = index;
        heap_sift_up(loop, index);
        heap_sift_down(loop, index);
    }
    timer->heap_index = EVL_NO_INDEX;
}

static void timer_link(evl_loop *loop, evl_timer *timer) {
    timer->prev = NULL;
    timer->next = loop->timers;
    if (loop->timers != NULL) {
        loop->timers->prev = timer;
    }
    loop->timers = timer;
}

static void timer_unlink(evl_loop *loop, evl_timer *timer) {
    if (timer->prev != NULL) {
        timer->prev->next = timer->next;
    } else {
        loop->timers = timer->next;
    }
    if (timer->next != NULL) {
        timer->next->prev = timer->prev;
    }
}

evl_timer *evl_add_timer(evl_loop *loop, int64_t delay_ms, int64_t repeat_ms,
                         evl_timer_cb cb, void *arg) {
    if (cb == NULL || delay_ms < 0 || repeat_ms < 0) {
        errno = EINVAL;
        return NULL;
    }
    evl_timer *timer = malloc(sizeof *timer);

    if (timer == NULL) {
        return NULL;
    }
    timer->when_ms = evl_now_ms() + delay_ms;
    timer->repeat_ms = repeat_ms;
    timer->cb = cb;
    timer->arg = arg;
    timer->heap_index = EVL_NO_INDEX;
    timer->prev = NULL;
    timer->next = NULL;
    if (heap_push(loop, timer) < 0) {
        free(timer);
        return NULL;
    }
    timer_link(loop, timer);
    return timer;
}

void evl_cancel_timer(evl_loop *loop, evl_timer *timer) {
    if (timer == NULL || timer->heap_index == EVL_NO_INDEX) {
        return;
    }
    heap_remove(loop, timer);
    timer_unlink(loop, timer);
    free(timer);
}

int evl_defer(evl_loop *loop, evl_defer_cb cb, void *arg) {
    if (cb == NULL) {
        errno = EINVAL;
        return -1;
    }
    struct evl_deferred *item = malloc(sizeof *item);

    if (item == NULL) {
        return -1;
    }
    item->cb = cb;
    item->arg = arg;
    item->next = NULL;
    if (loop->defer_tail != NULL) {
        loop->defer_tail->next = item;
    } else {
        loop->defer_head = item;
    }
    loop->defer_tail = item;
    return 0;
}

void evl_stop(evl_loop *loop) {
    loop->stop = 1;
}

static int compute_timeout(evl_loop *loop) {
    if (loop->defer_head != NULL) {
        return 0;
    }
    if (loop->heap_len == 0) {
        return -1;
    }
    int64_t delta = loop->heap[0]->when_ms - evl_now_ms();

    if (delta <= 0) {
        return 0;
    }
    if (delta > INT_MAX) {
        return INT_MAX;
    }
    return (int)delta;
}

static void dispatch_file_events(evl_loop *loop, int count) {
    for (int i = 0; i < count; i++) {
        int fd = loop->events[i].data.fd;

        if (fd < 0 || fd >= loop->fd_cap) {
            continue;
        }
        struct evl_fd *entry = loop->fds[fd];

        if (entry == NULL) {
            continue;
        }
        unsigned events = 0;
        uint32_t raw = loop->events[i].events;

        if (raw & EPOLLIN) {
            events |= EVL_READ;
        }
        if (raw & EPOLLOUT) {
            events |= EVL_WRITE;
        }
        if (raw & (EPOLLERR | EPOLLHUP)) {
            events |= entry->mask;
        }
        events &= entry->mask;
        if (events != 0) {
            entry->cb(loop, fd, events, entry->arg);
        }
    }
}

static void run_timers(evl_loop *loop) {
    for (;;) {
        int64_t now = evl_now_ms();

        if (loop->heap_len == 0 || loop->heap[0]->when_ms > now) {
            return;
        }
        evl_timer *timer = heap_pop(loop);
        int once = timer->repeat_ms == 0;

        if (!once) {
            timer->when_ms = now + timer->repeat_ms;
            if (heap_push(loop, timer) < 0) {
                timer_unlink(loop, timer);
                free(timer);
                continue;
            }
        }
        timer->cb(loop, timer, timer->arg);
        if (once) {
            timer_unlink(loop, timer);
            free(timer);
        }
    }
}

static void run_deferred(evl_loop *loop) {
    struct evl_deferred *item = loop->defer_head;

    loop->defer_head = NULL;
    loop->defer_tail = NULL;
    while (item != NULL) {
        struct evl_deferred *next = item->next;

        item->cb(loop, item->arg);
        free(item);
        item = next;
    }
}

int evl_run(evl_loop *loop) {
    loop->stop = 0;
    while (!loop->stop) {
        int timeout = compute_timeout(loop);
        int count = epoll_wait(loop->epfd, loop->events, EVL_MAX_EVENTS, timeout);

        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        dispatch_file_events(loop, count);
        run_timers(loop);
        run_deferred(loop);
    }
    return 0;
}