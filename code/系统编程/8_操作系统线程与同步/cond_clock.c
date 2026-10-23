#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WAIT_MS 300

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static long long now_ms(clockid_t clock) {
    struct timespec ts;

    clock_gettime(clock, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* deadline_clock 是写进 timespec 的那个时钟，必须与条件变量登记的一致。 */
static long long wait_ms(pthread_cond_t *cond, clockid_t deadline_clock) {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    struct timespec abs;
    long long start = now_ms(CLOCK_MONOTONIC);

    clock_gettime(deadline_clock, &abs);
    abs.tv_nsec += WAIT_MS * 1000000L;
    if (abs.tv_nsec >= 1000000000L) {
        abs.tv_sec += 1;
        abs.tv_nsec -= 1000000000L;
    }
    die_rc(pthread_mutex_lock(&lock), "pthread_mutex_lock");
    int rc = pthread_cond_timedwait(cond, &lock, &abs);
    die_rc(pthread_mutex_unlock(&lock), "pthread_mutex_unlock");
    if (rc != 0 && rc != ETIMEDOUT) {
        die_rc(rc, "pthread_cond_timedwait");
    }
    return now_ms(CLOCK_MONOTONIC) - start;
}

int main(void) {
    pthread_cond_t realtime = PTHREAD_COND_INITIALIZER;
    pthread_condattr_t attr;
    pthread_cond_t monotonic;

    die_rc(pthread_condattr_init(&attr), "pthread_condattr_init");
    die_rc(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC),
           "pthread_condattr_setclock");
    die_rc(pthread_cond_init(&monotonic, &attr), "pthread_cond_init");
    pthread_condattr_destroy(&attr);

    printf("CLOCK_REALTIME  = %lld ms\n", now_ms(CLOCK_REALTIME));
    printf("CLOCK_MONOTONIC = %lld ms\n", now_ms(CLOCK_MONOTONIC));
    printf("ask for %d ms in each case:\n", WAIT_MS);
    printf("default cond, REALTIME deadline:    %lld ms\n",
           wait_ms(&realtime, CLOCK_REALTIME));
    printf("default cond, MONOTONIC deadline:   %lld ms\n",
           wait_ms(&realtime, CLOCK_MONOTONIC));
    printf("MONOTONIC cond, MONOTONIC deadline: %lld ms\n",
           wait_ms(&monotonic, CLOCK_MONOTONIC));

    pthread_cond_destroy(&monotonic);
    pthread_cond_destroy(&realtime);
    return 0;
}