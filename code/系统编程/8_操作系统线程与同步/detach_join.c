#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void *idle(void *arg) {
    (void)arg;
    return NULL;
}

int main(void) {
    pthread_t t;

    pthread_create(&t, NULL, idle, NULL);
    pthread_detach(t);
    int rc = pthread_join(t, NULL);
    printf("join on detached thread: rc = %d (%s)\n", rc, strerror(rc));
    return 0;
}
