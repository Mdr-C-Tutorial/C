#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("xpd")
int xdp_pass(struct xdp_md *ctx)
{
    (void)ctx;
    return XDP_PASS;
}
