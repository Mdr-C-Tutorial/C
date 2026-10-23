#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREADS 2

static pthread_barrier_t barrier;
static int shared_fd = -1;

static void *worker(void *arg) {
    int id = (int)(long)arg;

    if (id == 0) {
        /* 打开的描述符属于整个进程，另一个线程马上就能用。 */
        shared_fd = open("thread.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (shared_fd == -1) {
            perror("open");
        }
    }
    pthread_barrier_wait(&barrier);

    if (shared_fd != -1) {
        dprintf(shared_fd, "thread %d writes through fd %d\n", id, shared_fd);
    }

    /* errno 是每个线程自己的：一个线程置了它，另一个线程看不到。 */
    errno = (id == 0) ? ENOENT : 0;
    pthread_barrier_wait(&barrier);
    printf("thread %d: errno = %d\n", id, errno);

    /* 信号掩码同样每线程一份，只在 thread 1 里阻塞 SIGUSR1。 */
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    if (id == 1) {
        pthread_sigmask(SIG_BLOCK, &set, NULL);
    }
    pthread_barrier_wait(&barrier);

    sigset_t current;

    pthread_sigmask(SIG_SETMASK, NULL, &current);
    printf("thread %d: SIGUSR1 blocked = %d, process id = %d\n", id,
           sigismember(&current, SIGUSR1), (int)getpid());
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];

    pthread_barrier_init(&barrier, NULL, THREADS);
    for (int i = 0; i < THREADS; i++) {
        pthread_create(&t[i], NULL, worker, (void *)(long)i);
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(t[i], NULL);
    }
    if (shared_fd != -1) {
        close(shared_fd);
    }
    pthread_barrier_destroy(&barrier);
    return 0;
}