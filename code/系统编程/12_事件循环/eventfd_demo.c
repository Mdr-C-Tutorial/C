#define _POSIX_C_SOURCE 200809L

#include "evl.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include <unistd.h>

static int wake_fd;

static void *worker(void *arg) {
    (void)arg;
    sleep(1);
    uint64_t one = 1;

    printf("工作线程完成，写 eventfd\n");
    fflush(stdout);
    if (write(wake_fd, &one, sizeof one) != sizeof one) {
        perror("write");
    }
    return NULL;
}

static void on_wake(evl_loop *loop, int fd, unsigned events, void *arg) {
    uint64_t count;

    (void)events;
    (void)arg;
    if (read(fd, &count, sizeof count) != sizeof count) {
        perror("read");
        return;
    }
    printf("循环线程被唤醒，计数 = %llu\n", (unsigned long long)count);
    evl_stop(loop);
}

int main(void) {
    evl_loop *loop = evl_create();
    pthread_t tid;

    wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd < 0) {
        perror("eventfd");
        return 1;
    }
    evl_add_fd(loop, wake_fd, EVL_READ, on_wake, NULL);
    pthread_create(&tid, NULL, worker, NULL);
    printf("循环线程进入 epoll_wait\n");
    evl_run(loop);
    pthread_join(tid, NULL);
    evl_destroy(loop);
    close(wake_fd);
    return 0;
}
