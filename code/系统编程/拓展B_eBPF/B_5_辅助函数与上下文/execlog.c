// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "execlog.skel.h"

static volatile sig_atomic_t exiting;

static void on_signal(int sig)
{
    (void)sig;
    exiting = 1;
}

static int on_libbpf_log(enum libbpf_print_level level, const char *format,
                         va_list args)
{
    if (level == LIBBPF_DEBUG) {
        return 0;
    }
    return vfprintf(stderr, format, args);
}

int main(void)
{
    struct execlog_bpf *skel;
    struct sigaction sa;
    int err;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) != 0 || sigaction(SIGTERM, &sa, NULL) != 0) {
        perror("sigaction");
        return 1;
    }

    libbpf_set_print(on_libbpf_log);

    skel = execlog_bpf__open();
    if (skel == NULL) {
        fprintf(stderr, "failed to open BPF skeleton\n");
        return 1;
    }

    err = execlog_bpf__load(skel);
    if (err != 0) {
        fprintf(stderr, "failed to load BPF skeleton: %s\n", strerror(-err));
        goto cleanup;
    }

    err = execlog_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "failed to attach BPF skeleton: %s\n", strerror(-err));
        goto cleanup;
    }

    printf("tracing exec... hit Ctrl-C to end\n");
    while (!exiting) {
        pause();
    }

cleanup:
    execlog_bpf__destroy(skel);
    return err != 0;
}