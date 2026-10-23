#define _POSIX_C_SOURCE 200809L

/* timerfd：定时器是一个描述符，read 得到自上次读以来到期的次数。
 * Linux 专有。 */
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void sleep_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
    }
}

int main(void) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (tfd == -1) {
        die("timerfd_create");
    }

    struct itimerspec spec = {0};
    spec.it_value.tv_nsec = 200 * 1000000L;
    spec.it_interval.tv_nsec = 200 * 1000000L;
    if (timerfd_settime(tfd, 0, &spec, NULL) == -1) {
        die("timerfd_settime");
    }

    sleep_ms(210);
    uint64_t expirations = 0;
    if (read(tfd, &expirations, sizeof expirations) !=
        (ssize_t)sizeof expirations) {
        die("read timerfd");
    }
    printf("first read: %llu\n", (unsigned long long)expirations);

    sleep_ms(650);
    if (read(tfd, &expirations, sizeof expirations) !=
        (ssize_t)sizeof expirations) {
        die("read timerfd");
    }
    printf("second read after 650 ms: %llu\n",
           (unsigned long long)expirations);

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    struct pollfd pfd = {tfd, POLLIN, 0};
    for (int i = 0; i < 2; i++) {
        int ready = poll(&pfd, 1, 2000);
        if (ready != 1) {
            die("poll");
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long elapsed_ms = (long long)(now.tv_sec - start.tv_sec) * 1000LL +
                               (now.tv_nsec - start.tv_nsec) / 1000000;
        printf("%lld ms: readable\n", elapsed_ms);
        if (read(tfd, &expirations, sizeof expirations) !=
            (ssize_t)sizeof expirations) {
            die("read timerfd");
        }
    }

    close(tfd);
    return 0;
}