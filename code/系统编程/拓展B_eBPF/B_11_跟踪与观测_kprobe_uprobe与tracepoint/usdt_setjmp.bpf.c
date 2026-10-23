#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/usdt.bpf.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 由用户态程序在 load 之前写入，自动附加没有限定进程，靠它做过滤 */
int target_pid = 0;

/* 节名写全了二进制、提供者和探针名，骨架的 __attach 会自动附加。
 * libc.so.6 不含斜杠，libbpf 会按动态库的搜索路径找到它。 */
SEC("usdt/libc.so.6:libc:setjmp")
int BPF_USDT(usdt_auto_attach, void *arg1, int arg2, void *arg3)
{
    pid_t pid = bpf_get_current_pid_tgid() >> 32;

    if (pid != target_pid) {
        return 0;
    }
    bpf_printk("USDT auto attach to libc:setjmp: arg1 = %lx, arg2 = %d, arg3 = %lx", arg1, arg2,
               arg3);
    return 0;
}

/* 只有 "usdt" 的节不能自动附加，由用户态用 bpf_program__attach_usdt 指定
 * 路径、提供者和探针名，并可以把 pid 一并传进去。 */
SEC("usdt")
int BPF_USDT(usdt_manual_attach, void *arg1, int arg2, void *arg3)
{
    bpf_printk("USDT manual attach to libc:setjmp: arg1 = %lx, arg2 = %d, arg3 = %lx", arg1,
               arg2, arg3);
    return 0;
}