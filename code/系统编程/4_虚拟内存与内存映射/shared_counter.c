#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

/* 用法：./shared_counter shared|private */
int main(int argc, char *argv[]) {
    int shared = argc > 1 && strcmp(argv[1], "shared") == 0;
    int flags = MAP_ANONYMOUS | (shared ? MAP_SHARED : MAP_PRIVATE);

    int *counter = mmap(NULL, sizeof *counter, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (counter == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    *counter = 0;

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        *counter = 42;
        _exit(0);
    }

    if (waitpid(pid, NULL, 0) == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    printf("子进程写了 42，父进程看到 %d\n", *counter);
    munmap(counter, sizeof *counter);
    return 0;
}
