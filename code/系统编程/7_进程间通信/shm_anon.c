#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

struct shared {
    long counter;
    char note[64];
};

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    struct shared *sh = mmap(NULL, sizeof *sh, PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sh == MAP_FAILED) {
        die("mmap");
    }
    sh->counter = 0;
    snprintf(sh->note, sizeof sh->note, "父进程初始化");

    pid_t pid = fork();
    if (pid == -1) {
        die("fork");
    }
    if (pid == 0) {
        for (int i = 0; i < 100; i++) {
            sh->counter++;
        }
        snprintf(sh->note, sizeof sh->note, "子进程写过");
        printf("子进程：counter = %ld\n", sh->counter);
        fflush(stdout);
        _exit(EXIT_SUCCESS);
    }

    if (waitpid(pid, NULL, 0) == -1) {
        die("waitpid");
    }
    printf("父进程：counter = %ld，note = %s\n", sh->counter, sh->note);

    /* 换成 MAP_PRIVATE 再试一次，父子各有一份副本 */
    struct shared *priv = mmap(NULL, sizeof *priv, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (priv == MAP_FAILED) {
        die("mmap private");
    }
    priv->counter = 0;
    snprintf(priv->note, sizeof priv->note, "父进程初始化");
    pid = fork();
    if (pid == -1) {
        die("fork");
    }
    if (pid == 0) {
        priv->counter = 100;
        snprintf(priv->note, sizeof priv->note, "子进程写过");
        _exit(EXIT_SUCCESS);
    }
    if (waitpid(pid, NULL, 0) == -1) {
        die("waitpid");
    }
    printf("MAP_PRIVATE：父进程看到 counter = %ld，note = %s\n", priv->counter,
           priv->note);

    munmap(sh, sizeof *sh);
    munmap(priv, sizeof *priv);
    return EXIT_SUCCESS;
}
