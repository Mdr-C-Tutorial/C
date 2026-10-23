#include "vmlinux.h"

#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("xdp")
int count_xdp(struct xdp_md *ctx)
{
    char comm[16];

    (void)ctx;
    bpf_get_current_comm(comm, sizeof(comm));
    return XDP_PASS;
}