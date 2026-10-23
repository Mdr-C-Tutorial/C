#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <threads.h>

#define THREADS 4
#define PER_THREAD 250000

static mtx_t lock;
static long counter;

static int worker(void *arg) {
    (void)arg;

    for (int i = 0; i < PER_THREAD; i++) {
        if (mtx_lock(&lock) != thrd_success) {
            return 1;
        }
        counter++;
        mtx_unlock(&lock);
    }
    return 0;
}

int main(void) {
    thrd_t t[THREADS];

    if (mtx_init(&lock, mtx_plain) != thrd_success) {
        return 1;
    }
    for (int i = 0; i < THREADS; i++) {
        if (thrd_create(&t[i], worker, NULL) != thrd_success) {
            return 1;
        }
    }
    for (int i = 0; i < THREADS; i++) {
        thrd_join(t[i], NULL);
    }
    printf("counter = %ld\n", counter);
    mtx_destroy(&lock);
    return 0;
}