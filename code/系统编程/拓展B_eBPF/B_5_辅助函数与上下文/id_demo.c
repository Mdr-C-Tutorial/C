#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

static void *worker(void *arg)
{
    (void)arg;
    printf("thread: getpid() = %ld, gettid() = %ld\n", (long)getpid(),
           (long)syscall(SYS_gettid));
    return NULL;
}

int main(void)
{
    pthread_t th;

    printf("main:   getpid() = %ld, gettid() = %ld\n", (long)getpid(),
           (long)syscall(SYS_gettid));
    pthread_create(&th, NULL, worker, NULL);
    pthread_join(th, NULL);
    return 0;
}