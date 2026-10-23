// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 旧内核把这个成员叫 state，新内核叫 __state。
 * 名字里带 ___ 后缀的部分在匹配目标内核时被忽略：
 * 这个类型会被当成 struct task_struct，成员名按 state 去找。 */
struct task_struct___old {
    long state;
} __attribute__((preserve_access_index));

SEC("tp/sched/sched_process_exec")
int handle_exec(struct trace_event_raw_sched_process_exec *ctx)
{
    struct task_struct *task;
    long state;
    pid_t ppid;

    task = (struct task_struct *)bpf_get_current_task();
    ppid = BPF_CORE_READ(task, real_parent, tgid);

    if (bpf_core_field_exists(task->__state)) {
        state = BPF_CORE_READ(task, __state);
    } else {
        state = BPF_CORE_READ((struct task_struct___old *)task, state);
    }

    bpf_printk("pid = %d, ppid = %d, state = %ld\n", ctx->pid, ppid, state);
    return 0;
}