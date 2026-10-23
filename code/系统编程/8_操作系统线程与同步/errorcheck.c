#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    pthread_mutex_t m;
    pthread_mutexattr_t a;

    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&m, &a);
    pthread_mutexattr_destroy(&a);

    int rc = pthread_mutex_lock(&m);
    printf("first lock:   rc = %d (%s)\n", rc, strerror(rc));
    rc = pthread_mutex_lock(&m);
    printf("second lock:  rc = %d (%s)\n", rc, strerror(rc));
    rc = pthread_mutex_unlock(&m);
    printf("first unlock: rc = %d (%s)\n", rc, strerror(rc));
    rc = pthread_mutex_unlock(&m);
    printf("second unlock: rc = %d (%s)\n", rc, strerror(rc));

    pthread_mutex_destroy(&m);
    return 0;
}