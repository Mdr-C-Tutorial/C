#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bpf/libbpf.h>
#include "connect_deny.skel.h"

static volatile sig_atomic_t exiting;

static void handle_signal(int signo) {
    (void)signo;
    exiting = 1;
}

int main(int argc, char *argv[]) {
    const char *cgroup_path = "/sys/fs/cgroup/mdr-ebpf";
    struct connect_deny *skel;
    struct bpf_link *link;
    int cgroup_fd;
    int err;

    if (argc > 1) {
        cgroup_path = argv[1];
    }

    skel = connect_deny__open();
    if (skel == NULL) {
        fprintf(stderr, "failed to open skeleton\n");
        return 1;
    }

    err = connect_deny__load(skel);
    if (err != 0) {
        fprintf(stderr, "failed to load: %s\n", strerror(-err));
        connect_deny__destroy(skel);
        return 1;
    }

    cgroup_fd = open(cgroup_path, O_RDONLY | O_DIRECTORY);
    if (cgroup_fd < 0) {
        fprintf(stderr, "open %s: %s\n", cgroup_path, strerror(errno));
        connect_deny__destroy(skel);
        return 1;
    }

    link = bpf_program__attach_cgroup(skel->progs.connect_deny, cgroup_fd);
    if (link == NULL) {
        fprintf(stderr, "failed to attach: %s\n", strerror(errno));
        close(cgroup_fd);
        connect_deny__destroy(skel);
        return 1;
    }

    struct sigaction sa = {
        .sa_handler = handle_signal,
    };
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("connect_deny attached to %s, deny port %u\n", cgroup_path,
           skel->rodata->deny_port);
    while (!exiting) {
        sleep(1);
    }

    bpf_link__destroy(link);
    close(cgroup_fd);
    connect_deny__destroy(skel);
    return 0;
}
