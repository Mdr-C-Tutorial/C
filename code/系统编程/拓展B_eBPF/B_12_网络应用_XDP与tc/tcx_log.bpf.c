#include "vmlinux.h"

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

#define ETH_P_IP 0x0800

char __license[] SEC("license") = "Dual BSD/GPL";

SEC("tcx/ingress")
int tcx_log_ingress(struct __sk_buff *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;
    struct iphdr *ip;

    if (ctx->protocol != bpf_htons(ETH_P_IP)) {
        return TCX_PASS;
    }

    if ((void *)(eth + 1) > data_end) {
        return TCX_PASS;
    }

    ip = (struct iphdr *)(eth + 1);
    if ((void *)(ip + 1) > data_end) {
        return TCX_PASS;
    }

    bpf_printk("Got IP packet: tot_len: %d, ttl: %d", bpf_ntohs(ip->tot_len),
               ip->ttl);
    return TCX_PASS;
}