#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

/* main 调用 pthread_exit 后进程不结束，工作线程继续运行。 */
static void *worker(void *arg) {
    (void)arg;

    for (int i = 1; i <= 3; i++) {
        printf("worker %d\n", i);
        fflush(stdout);
        sleep(1);
    }
    return NULL;
}

int main(void) {
    pthread_t t;

    pthread_create(&t, NULL, worker, NULL);
    pthread_exit(NULL);
}
