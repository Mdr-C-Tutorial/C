#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "tcx_log.skel.h"

static volatile sig_atomic_t exiting;

static void handle_signal(int sig) {
    (void)sig;
    exiting = 1;
}

static int install_signals(void) {
    struct sigaction sa = {.sa_handler = handle_signal};

    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) != 0 ||
        sigaction(SIGTERM, &sa, NULL) != 0) {
        perror("sigaction");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *ifname = "lo";
    struct tcx_log_bpf *skel = NULL;
    struct bpf_link *link = NULL;
    int opt;
    int err;

    while ((opt = getopt(argc, argv, "i:h")) != -1) {
        switch (opt) {
        case 'i':
            ifname = optarg;
            break;
        default:
            fprintf(stderr, "用法：%s [-i 网卡]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (install_signals() != 0) {
        return EXIT_FAILURE;
    }

    skel = tcx_log_bpf__open();
    if (skel == NULL) {
        fprintf(stderr, "打开骨架失败\n");
        return EXIT_FAILURE;
    }

    err = tcx_log_bpf__load(skel);
    if (err != 0) {
        fprintf(stderr, "加载失败：%s\n", strerror(-err));
        goto out;
    }

    int ifindex = (int)if_nametoindex(ifname);
    if (ifindex == 0) {
        perror(ifname);
        goto out;
    }

    link = bpf_program__attach_tcx(skel->progs.tcx_log_ingress, ifindex, NULL);
    err = libbpf_get_error(link);
    if (err != 0) {
        fprintf(stderr, "附加失败：%s\n", strerror(-err));
        link = NULL;
        goto out;
    }

    printf("已在 %s 的入口附加 tcx 程序，输出在 trace_pipe 中，按 Ctrl-C 退出\n",
           ifname);
    fflush(stdout);

    while (!exiting) {
        pause();
    }

out:
    bpf_link__destroy(link);
    tcx_log_bpf__destroy(skel);
    return err == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}