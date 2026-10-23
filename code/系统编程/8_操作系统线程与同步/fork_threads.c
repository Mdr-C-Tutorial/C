#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define THREADS 3

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static int task_count(void) {
    DIR *dir = opendir("/proc/self/task");
    struct dirent *ent;
    int n = 0;

    if (dir == NULL) {
        return -1;
    }
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] != '.') {
            n++;
        }
    }
    closedir(dir);
    return n;
}

static void *idle(void *arg) {
    (void)arg;
    sleep(2);
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];

    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_create(&t[i], NULL, idle, NULL), "pthread_create");
    }
    printf("parent: %d threads\n", task_count());

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        /* 子进程里只有调用 fork 的那个线程；这里只能调用异步信号安全的函数。 */
        char buf[64];
        int n = snprintf(buf, sizeof buf, "child:  %d thread\n", task_count());

        write(STDOUT_FILENO, buf, (size_t)n);
        _exit(0);
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_join(t[i], NULL), "pthread_join");
    }
    return 0;
}