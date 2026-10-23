#include "vmlinux.h"

#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "MIT";

SEC("tp/sched/sched_process_exec")
int count_exec(void *ctx)
{
    struct task_struct *task;

    (void)ctx;
    task = bpf_get_current_task_btf();
    (void)task;
    return 0;
}