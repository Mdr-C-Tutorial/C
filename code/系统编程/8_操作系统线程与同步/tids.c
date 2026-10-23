#define _GNU_SOURCE

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define THREADS 3

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static void sleep_ms(long ms) {
    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};

    while (nanosleep(&ts, &ts) == -1) {
    }
}

static void list_tasks(void) {
    DIR *dir = opendir("/proc/self/task");
    struct dirent *ent;

    if (dir == NULL) {
        perror("opendir");
        return;
    }
    printf("/proc/%d/task:", (int)getpid());
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] != '.') {
            printf(" %s", ent->d_name);
        }
    }
    printf("\n");
    closedir(dir);
}

static void *worker(void *arg) {
    const char *name = arg;

    die_rc(pthread_setname_np(pthread_self(), name), "pthread_setname_np");
    printf("  %-5s pthread_self = %lu, gettid = %ld\n", name,
           (unsigned long)pthread_self(), (long)gettid());
    fflush(stdout);
    sleep_ms(2000);
    return NULL;
}

int main(void) {
    static const char *names[THREADS] = {"alpha", "beta", "gamma"};
    pthread_t t[THREADS];

    printf("main:  pthread_self = %lu, gettid = %ld, getpid = %d\n",
           (unsigned long)pthread_self(), (long)gettid(), (int)getpid());
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_create(&t[i], NULL, worker, (void *)names[i]), "pthread_create");
    }
    sleep_ms(300);
    list_tasks();
    fflush(stdout);
    for (int i = 0; i < THREADS; i++) {
        die_rc(pthread_join(t[i], NULL), "pthread_join");
    }
    return 0;
}