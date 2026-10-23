#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bpf/libbpf.h>
#include "lsm_bpf.skel.h"

static volatile sig_atomic_t exiting;

static void handle_signal(int signo) {
    (void)signo;
    exiting = 1;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
                           va_list args) {
    if (level == LIBBPF_DEBUG) {
        return 0;
    }
    return vfprintf(stderr, format, args);
}

int main(void) {
    struct lsm_bpf *skel;
    struct bpf_link *link;
    int err;

    libbpf_set_print(libbpf_print_fn);

    skel = lsm_bpf__open();
    if (skel == NULL) {
        fprintf(stderr, "failed to open skeleton\n");
        return 1;
    }

    err = lsm_bpf__load(skel);
    if (err != 0) {
        fprintf(stderr, "failed to load: %s\n", strerror(-err));
        lsm_bpf__destroy(skel);
        return 1;
    }

    link = bpf_program__attach_lsm(skel->progs.lsm_bpf);
    if (link == NULL) {
        fprintf(stderr, "failed to attach: %s\n", strerror(errno));
        lsm_bpf__destroy(skel);
        return 1;
    }

    struct sigaction sa = {
        .sa_handler = handle_signal,
    };
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("lsm_bpf attached, press Ctrl-C to stop\n");
    while (!exiting) {
        sleep(1);
    }

    bpf_link__destroy(link);
    lsm_bpf__destroy(skel);
    return 0;
}
