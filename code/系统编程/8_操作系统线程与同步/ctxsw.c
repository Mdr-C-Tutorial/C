#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#define ROUNDS 5
#define WORK_MS 40

struct worker {
    const char *name;
    int busy;
    unsigned long vol;
    unsigned long invol;
    double cpu_ms;
};

static void die_rc(int rc, const char *what) {
    if (rc == 0) {
        return;
    }
    fprintf(stderr, "%s: %s\n", what, strerror(rc));
    exit(EXIT_FAILURE);
}

static long long now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static double ms_of(const struct timeval *tv) {
    return (double)tv->tv_sec * 1000.0 + (double)tv->tv_usec / 1000.0;
}

/* 线程还活着时读自己的 /proc/self/task/<tid>/status。 */
static void read_switches(struct worker *w) {
    char path[64];
    char line[256];

    snprintf(path, sizeof path, "/proc/self/task/%ld/status",
             (long)gettid());
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror(path);
        return;
    }
    while (fgets(line, sizeof line, fp) != NULL) {
        unsigned long value;

        if (sscanf(line, "voluntary_ctxt_switches: %lu", &value) == 1) {
            w->vol = value;
        } else if (sscanf(line, "nonvoluntary_ctxt_switches: %lu", &value) == 1) {
            w->invol = value;
        }
    }
    fclose(fp);
}

static void *work(void *arg) {
    struct worker *w = arg;
    struct rusage ru;

    for (int i = 0; i < ROUNDS; i++) {
        long long deadline = now_ms() + WORK_MS;

        if (w->busy) {
            while (now_ms() < deadline) {
            }
        } else {
            struct timespec ts = {.tv_sec = 0, .tv_nsec = WORK_MS * 1000000L};

            while (nanosleep(&ts, &ts) == -1) {
            }
        }
    }
    read_switches(w);
    if (getrusage(RUSAGE_THREAD, &ru) != 0) {
        perror("getrusage");
        return NULL;
    }
    w->cpu_ms = ms_of(&ru.ru_utime) + ms_of(&ru.ru_stime);
    return NULL;
}

int main(void) {
    struct worker sleep_worker = {"sleep", 0, 0, 0, 0.0};
    struct worker busy_worker = {"busy", 1, 0, 0, 0.0};
    pthread_t a;
    pthread_t b;

    die_rc(pthread_create(&a, NULL, work, &sleep_worker), "pthread_create");
    die_rc(pthread_create(&b, NULL, work, &busy_worker), "pthread_create");
    die_rc(pthread_join(a, NULL), "pthread_join");
    die_rc(pthread_join(b, NULL), "pthread_join");
    printf("%d rounds of %d ms each:\n", ROUNDS, WORK_MS);
    printf("%-5s cpu = %7.2f ms  voluntary = %-2lu nonvoluntary = %lu\n",
           sleep_worker.name, sleep_worker.cpu_ms, sleep_worker.vol,
           sleep_worker.invol);
    printf("%-5s cpu = %7.2f ms  voluntary = %-2lu nonvoluntary = %lu\n",
           busy_worker.name, busy_worker.cpu_ms, busy_worker.vol,
           busy_worker.invol);
    return 0;
}