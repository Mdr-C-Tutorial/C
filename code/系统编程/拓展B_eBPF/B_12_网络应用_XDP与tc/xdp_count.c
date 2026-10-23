#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "xdp_count.skel.h"

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

static const char *proto_name(unsigned int proto) {
    switch (proto) {
    case IPPROTO_ICMP:
        return "ICMP";
    case IPPROTO_TCP:
        return "TCP";
    case IPPROTO_UDP:
        return "UDP";
    default:
        return "其它";
    }
}

static void print_counts(struct xdp_count_bpf *skel,
                         unsigned long long *values, int nr_cpus) {
    int fd = bpf_map__fd(skel->maps.proto_count);

    for (unsigned int proto = 0; proto < 256; proto++) {
        unsigned long long total = 0;

        if (bpf_map_lookup_elem(fd, &proto, values) != 0) {
            continue;
        }
        for (int i = 0; i < nr_cpus; i++) {
            total += values[i];
        }
        if (total != 0) {
            printf("  协议 %u（%s）：%llu 个报文\n", proto, proto_name(proto),
                   total);
        }
    }
    fflush(stdout);
}

int main(int argc, char **argv) {
    const char *ifname = "lo";
    unsigned int drop_port = 20001;
    struct xdp_count_bpf *skel = NULL;
    struct bpf_link *link = NULL;
    unsigned long long *values = NULL;
    int nr_cpus;
    int err = 0;
    int opt;

    while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
        switch (opt) {
        case 'i':
            ifname = optarg;
            break;
        case 'p':
            drop_port = (unsigned int)strtoul(optarg, NULL, 10);
            break;
        default:
            fprintf(stderr, "用法：%s [-i 网卡] [-p 丢弃端口]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (install_signals() != 0) {
        return EXIT_FAILURE;
    }

    nr_cpus = libbpf_num_possible_cpus();
    if (nr_cpus < 0) {
        fprintf(stderr, "读取 CPU 数量失败：%s\n", strerror(-nr_cpus));
        return EXIT_FAILURE;
    }
    values = calloc((size_t)nr_cpus, sizeof(*values));
    if (values == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    skel = xdp_count_bpf__open();
    if (skel == NULL) {
        free(values);
        return EXIT_FAILURE;
    }

    skel->rodata->drop_port = drop_port;
    err = xdp_count_bpf__load(skel);
    if (err != 0) {
        fprintf(stderr, "加载失败：%s\n", strerror(-err));
        goto out;
    }

    int ifindex = (int)if_nametoindex(ifname);
    if (ifindex == 0) {
        fprintf(stderr, "找不到网卡 %s\n", ifname);
        err = -1;
        goto out;
    }

    link = bpf_program__attach_xdp(skel->progs.xdp_count_proto, ifindex);
    err = libbpf_get_error(link);
    if (err != 0) {
        fprintf(stderr, "附加失败：%s\n", strerror(-err));
        link = NULL;
        goto out;
    }

    printf("已在 %s 上附加 XDP 程序，丢弃目的端口 %u 的 UDP 报文，按 Ctrl-C 退出\n",
           ifname, drop_port);
    fflush(stdout);

    while (!exiting) {
        sleep(1);
        print_counts(skel, values, nr_cpus);
    }

    printf("卸载 XDP 程序\n");

out:
    bpf_link__destroy(link);
    xdp_count_bpf__destroy(skel);
    free(values);
    return err == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}