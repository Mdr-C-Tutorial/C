#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 用户态可以读写的全局对象，落在 .bss */
int target_pid = 0;

/* 只读参数，落在 .rodata；用户态在 load 之前写入 */
const volatile __u64 threshold_ns = 1000000;

/* 只在程序内部使用，用户态看不到 */
static int hits = 0;

SEC("tp/sched/sched_process_exec")
int handle_exec(void *ctx)
{
    __u32 pid = bpf_get_current_pid_tgid() >> 32;

    (void)ctx;
    if (target_pid != 0 && pid != (__u32)target_pid) {
        return 0;
    }
    hits += 1;
    return 0;
}