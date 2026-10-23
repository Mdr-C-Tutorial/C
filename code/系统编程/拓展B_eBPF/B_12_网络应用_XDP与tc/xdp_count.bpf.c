#include "vmlinux.h"

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

#define ETH_P_IP 0x0800

char __license[] SEC("license") = "Dual BSD/GPL";

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 256);
    __type(key, __u32);
    __type(value, __u64);
} proto_count SEC(".maps");

const volatile __u32 drop_port = 20001;

SEC("xdp")
int xdp_count_proto(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;
    struct iphdr *ip;
    struct udphdr *udp;
    __u32 key;
    __u64 *count;

    if ((void *)(eth + 1) > data_end) {
        return XDP_PASS;
    }
    if (eth->h_proto != bpf_htons(ETH_P_IP)) {
        return XDP_PASS;
    }

    ip = (struct iphdr *)(eth + 1);
    if ((void *)(ip + 1) > data_end) {
        return XDP_PASS;
    }
    if (ip->ihl < 5) {
        return XDP_PASS;
    }

    key = ip->protocol;
    count = bpf_map_lookup_elem(&proto_count, &key);
    if (count != NULL) {
        *count += 1;
    }

    if (ip->protocol != IPPROTO_UDP) {
        return XDP_PASS;
    }

    udp = (struct udphdr *)((char *)ip + ip->ihl * 4);
    if ((void *)(udp + 1) > data_end) {
        return XDP_PASS;
    }
    if (bpf_ntohs(udp->dest) == drop_port) {
        return XDP_DROP;
    }
    return XDP_PASS;
}