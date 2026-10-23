#define _GNU_SOURCE

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

/* 演示 pthread 的错误约定：错误号由返回值给出，errno 不动。 */
int main(void) {
    pthread_mutex_t m;
    pthread_mutexattr_t a;

    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&m, &a);
    pthread_mutexattr_destroy(&a);
    pthread_mutex_lock(&m);

    errno = 0;
    int rc = pthread_mutex_lock(&m);
    printf("rc = %d (%s), errno = %d\n", rc, strerror(rc), errno);

    rc = pthread_setname_np(pthread_self(), "a-name-that-is-too-long");
    printf("setname rc = %d (%s)\n", rc, strerror(rc));
    return 0;
}
