#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "syscount.skel.h"

#define COMM_LEN 16
#define MAX_ENTRIES 10240
#define TOP_N 10

static volatile sig_atomic_t stop;

static void on_signal(int signo)
{
    (void)signo;
    stop = 1;
}

static int print_libbpf(enum libbpf_print_level level, const char *format, va_list args)
{
    (void)level;
    return vfprintf(stderr, format, args);
}

struct entry {
    char comm[COMM_LEN];
    __u64 count;
};

static int compare_count(const void *a, const void *b)
{
    const struct entry *x = a;
    const struct entry *y = b;

    if (x->count < y->count) {
        return 1;
    }
    if (x->count > y->count) {
        return -1;
    }
    return 0;
}

/* 逐个取出最前面的键，读出计数值后删掉它，顺带把 map 清零 */
static size_t drain(const struct bpf_map *map, struct entry *all)
{
    char key[COMM_LEN];
    size_t n = 0;

    while (n < MAX_ENTRIES &&
           bpf_map__get_next_key(map, NULL, key, sizeof(key)) == 0) {
        all[n].count = 0;
        if (bpf_map__lookup_elem(map, key, sizeof(key), &all[n].count,
                                 sizeof(all[n].count), 0) != 0) {
            all[n].count = 0;
        }
        memcpy(all[n].comm, key, sizeof(key));
        n++;
        bpf_map__delete_elem(map, key, sizeof(key), 0);
    }
    return n;
}

static void print_top(const struct bpf_map *map, struct entry *all)
{
    size_t n;
    size_t i;

    n = drain(map, all);
    qsort(all, n, sizeof(all[0]), compare_count);
    for (i = 0; i < n && i < TOP_N; i++) {
        printf("%-16s %llu\n", all[i].comm, (unsigned long long)all[i].count);
    }
    fflush(stdout);
}

int main(void)
{
    struct syscount_bpf *skel;
    struct sigaction sa;
    struct entry *all;
    int err;

    libbpf_set_print(print_libbpf);

    all = calloc(MAX_ENTRIES, sizeof(*all));
    if (all == NULL) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }

    skel = syscount_bpf__open_and_load();
    if (skel == NULL) {
        fprintf(stderr, "无法打开并加载骨架\n");
        free(all);
        return 1;
    }

    err = syscount_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "附加失败：%s\n", strerror(-err));
        goto cleanup;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    while (!stop) {
        sleep(2);
        printf("%-16s %s\n", "COMM", "SYSCALLS");
        print_top(skel->maps.counts, all);
    }

cleanup:
    syscount_bpf__destroy(skel);
    free(all);
    return err != 0;
}