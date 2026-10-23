#include "vmlinux.h"

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

#define ETH_P_IP 0x0800

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("xdp")
int xdp_peek(struct xdp_md *ctx)
{
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;

    if (eth->h_proto == bpf_htons(ETH_P_IP)) {
        return XDP_DROP;
    }
    return XDP_PASS;
}