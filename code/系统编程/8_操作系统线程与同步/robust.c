#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

struct shared {
    pthread_mutex_t lock;
    int counter;
};

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

int main(void) {
    struct shared *sh = mmap(NULL, sizeof *sh, PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sh == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    pthread_mutexattr_t attr;
    die_rc(pthread_mutexattr_init(&attr), "pthread_mutexattr_init");
    die_rc(pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED),
           "pthread_mutexattr_setpshared");
    die_rc(pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST),
           "pthread_mutexattr_setrobust");
    die_rc(pthread_mutex_init(&sh->lock, &attr), "pthread_mutex_init");
    pthread_mutexattr_destroy(&attr);
    sh->counter = 1;

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        /* 子进程拿到锁后不释放就退出，模拟崩溃。 */
        die_rc(pthread_mutex_lock(&sh->lock), "child lock");
        sh->counter = 99;
        _exit(0);
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    int rc = pthread_mutex_lock(&sh->lock);
    printf("parent lock after child crash: rc = %d (%s)\n", rc, strerror(rc));
    if (rc == EOWNERDEAD) {
        die_rc(pthread_mutex_consistent(&sh->lock), "pthread_mutex_consistent");
        printf("counter left by child = %d, marked consistent\n", sh->counter);
        die_rc(pthread_mutex_unlock(&sh->lock), "pthread_mutex_unlock");
    }
    pthread_mutex_destroy(&sh->lock);
    munmap(sh, sizeof *sh);
    return 0;
}