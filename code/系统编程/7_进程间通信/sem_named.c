#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define SEM_NAME "/p087_sem"

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    sem_t *sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0600, 0);
    if (sem == SEM_FAILED) {
        die("sem_open");
    }

    pid_t pid = fork();
    if (pid == -1) {
        die("fork");
    }
    if (pid == 0) {
        sleep(1);
        if (sem_post(sem) == -1) {
            die("sem_post");
        }
        _exit(EXIT_SUCCESS);
    }

    if (sem_wait(sem) == -1) {
        die("sem_wait");
    }
    puts("父进程等到了子进程的 sem_post");
    if (waitpid(pid, NULL, 0) == -1) {
        die("waitpid");
    }
    sem_close(sem);
    sem_unlink(SEM_NAME);
    return EXIT_SUCCESS;
}
