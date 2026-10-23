#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "openstat.skel.h"

static volatile sig_atomic_t stop;

static void on_signal(int signo) {
    (void)signo;
    stop = 1;
}

static void dump_opens(struct openstat_bpf *skel) {
    struct bpf_map *map = skel->maps.opens;
    char key[16];
    char next[16];
    const char *cur = NULL;
    unsigned long long value;
    int err;

    for (;;) {
        err = bpf_map__get_next_key(map, cur, next, sizeof(next));
        if (err != 0) {
            break;
        }
        err = bpf_map__lookup_elem(map, next, sizeof(next), &value,
                                   sizeof(value), 0);
        if (err == 0) {
            printf("%-16.16s %llu\n", next, value);
        }
        memcpy(key, next, sizeof(key));
        cur = key;
    }
}

int main(void) {
    struct sigaction sa;
    struct openstat_bpf *skel;
    int err;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    skel = openstat_bpf__open_and_load();
    if (skel == NULL) {
        fprintf(stderr, "failed to open and load BPF object\n");
        return 1;
    }
    err = openstat_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "failed to attach: %s\n", strerror(-err));
        openstat_bpf__destroy(skel);
        return 1;
    }

    printf("Tracing openat... Hit Ctrl-C to end.\n");
    while (!stop) {
        pause();
    }

    dump_opens(skel);
    openstat_bpf__destroy(skel);
    return 0;
}
