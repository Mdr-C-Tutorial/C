#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tp/syscalls/sys_enter_write")
int tp_write(void *ctx)
{
    (void)ctx;
    return 0;
}

SEC("kprobe/do_unlinkat")
int kp_unlink(void *ctx)
{
    (void)ctx;
    return 0;
}

SEC("fentry/do_unlinkat")
int BPF_PROG(fe_unlink, struct filename *name)
{
    (void)ctx;
    (void)name;
    return 0;
}

SEC("xdp")
int xdp_pass(struct xdp_md *ctx)
{
    (void)ctx;
    return XDP_PASS;
}

SEC("tcx/ingress")
int tcx_ingress(struct __sk_buff *skb)
{
    (void)skb;
    return TCX_PASS;
}

SEC("cgroup_skb/egress")
int cg_egress(struct __sk_buff *skb)
{
    (void)skb;
    return 1;
}

SEC("lsm/file_mprotect")
int BPF_PROG(lsm_mprotect, struct vm_area_struct *vma, unsigned long reqprot,
             unsigned long prot, int ret)
{
    (void)ctx;
    (void)vma;
    (void)reqprot;
    (void)prot;
    (void)ret;
    return 0;
}
