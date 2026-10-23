// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tp/sched/sched_process_exec")
int handle_exec(struct trace_event_raw_sched_process_exec *ctx)
{
    struct task_struct *task;
    char filename[256];
    char comm[16];
    __u64 id;
    __u32 uid;
    __u32 offset;
    pid_t pid;
    pid_t ppid;

    id = bpf_get_current_pid_tgid();
    pid = (pid_t)(id >> 32);
    uid = (__u32)bpf_get_current_uid_gid();
    bpf_get_current_comm(comm, sizeof(comm));

    task = (struct task_struct *)bpf_get_current_task();
    if (task == 0) {
        return 0;
    }
    ppid = (pid_t)BPF_CORE_READ(task, real_parent, tgid);

    offset = ctx->__data_loc_filename & 0xFFFF;
    bpf_probe_read_kernel_str(filename, sizeof(filename),
                              (const char *)ctx + offset);

    bpf_printk("EXEC pid=%d ppid=%d uid=%u comm=%s file=%s\n", pid, ppid, uid,
               comm, filename);
    return 0;
}