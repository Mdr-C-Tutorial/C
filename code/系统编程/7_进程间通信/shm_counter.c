#define _POSIX_C_SOURCE 200809L

#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define INCREMENTS 1000000
#define WORKERS 2

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 共享区：一个计数，一个二值信号量（只有 sem 模式才初始化） */
struct shared {
    long counter;
    sem_t lock;
};

static void bump(struct shared *sh, int mode) {
    for (int i = 0; i < INCREMENTS; i++) {
        if (mode == 0) {
            sh->counter++; /* 读-改-写，没有任何保护 */
        } else if (mode == 1) {
            sem_wait(&sh->lock);
            sh->counter++;
            sem_post(&sh->lock);
        } else {
            /* 用 C11 原子操作，留作对比 */
            atomic_fetch_add_explicit((atomic_long *)&sh->counter, 1,
                                      memory_order_relaxed);
        }
    }
}

int main(int argc, char *argv[]) {
    int mode = 0;
    if (argc > 1) {
        if (strcmp(argv[1], "sem") == 0) {
            mode = 1;
        } else if (strcmp(argv[1], "atomic") == 0) {
            mode = 2;
        }
    }

    struct shared *sh = mmap(NULL, sizeof *sh, PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sh == MAP_FAILED) {
        die("mmap");
    }
    sh->counter = 0;
    if (mode == 1 && sem_init(&sh->lock, 1, 1) == -1) {
        die("sem_init");
    }

    for (int i = 0; i < WORKERS; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            die("fork");
        }
        if (pid == 0) {
            bump(sh, mode);
            _exit(EXIT_SUCCESS);
        }
    }
    for (int i = 0; i < WORKERS; i++) {
        if (wait(NULL) == -1) {
            die("wait");
        }
    }

    printf("两个进程各加 %d 次，计数器 = %ld（应为 %d）\n", INCREMENTS,
           sh->counter, INCREMENTS * WORKERS);
    if (mode == 1) {
        sem_destroy(&sh->lock);
    }
    munmap(sh, sizeof *sh);
    return EXIT_SUCCESS;
}
