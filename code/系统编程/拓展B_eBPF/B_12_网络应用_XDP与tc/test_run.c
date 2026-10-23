#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "xdp_count.skel.h"

static size_t build_frame(unsigned char *frame, size_t cap,
                          unsigned short dest_port) {
    struct ethhdr *eth = (struct ethhdr *)frame;
    struct iphdr *ip = (struct iphdr *)(eth + 1);
    struct udphdr *udp = (struct udphdr *)((unsigned char *)ip + 20);
    const char *payload = "test";
    size_t len = sizeof(*eth) + 20 + sizeof(*udp) + strlen(payload);

    if (len > cap) {
        return 0;
    }
    memset(frame, 0, cap);
    eth->h_proto = htons(ETH_P_IP);
    ip->version = 4;
    ip->ihl = 5;
    ip->tot_len = htons((unsigned short)(20 + sizeof(*udp) + strlen(payload)));
    ip->ttl = 64;
    ip->protocol = IPPROTO_UDP;
    ip->saddr = htonl(0x7f000001u);
    ip->daddr = htonl(0x7f000001u);
    udp->source = htons(40000);
    udp->dest = htons(dest_port);
    udp->len = htons((unsigned short)(sizeof(*udp) + strlen(payload)));
    memcpy((unsigned char *)udp + sizeof(*udp), payload, strlen(payload));
    return len;
}

static int run_one(struct bpf_program *prog, unsigned short port) {
    unsigned char frame[128];
    LIBBPF_OPTS(bpf_test_run_opts, opts, .data_in = frame, .repeat = 1);
    int err;

    opts.data_size_in = (unsigned int)build_frame(frame, sizeof frame, port);
    err = bpf_prog_test_run_opts(bpf_program__fd(prog), &opts);
    if (err != 0) {
        fprintf(stderr, "bpf_prog_test_run_opts：%s\n", strerror(-err));
        return -1;
    }
    printf("目的端口 %u：返回值 %u\n", port, opts.retval);
    return 0;
}

int main(void) {
    struct xdp_count_bpf *skel;
    int err;

    skel = xdp_count_bpf__open();
    if (skel == NULL) {
        return EXIT_FAILURE;
    }
    skel->rodata->drop_port = 20001;
    err = xdp_count_bpf__load(skel);
    if (err != 0) {
        fprintf(stderr, "加载失败：%s\n", strerror(-err));
        xdp_count_bpf__destroy(skel);
        return EXIT_FAILURE;
    }

    if (run_one(skel->progs.xdp_count_proto, 20001) != 0 ||
        run_one(skel->progs.xdp_count_proto, 20002) != 0) {
        xdp_count_bpf__destroy(skel);
        return EXIT_FAILURE;
    }

    xdp_count_bpf__destroy(skel);
    return EXIT_SUCCESS;
}