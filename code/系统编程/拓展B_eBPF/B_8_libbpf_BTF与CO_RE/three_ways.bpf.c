// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* 读取 real_parent->tgid 的三种写法，供正文对照。
 * 第四个程序读用户空间内存，留给习题补完。
 * 加载程序见 three_ways.c。 */
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 一：手写两次 bpf_probe_read_kernel */
SEC("tp/sched/sched_process_exec")
int manual(struct trace_event_raw_sched_process_exec *ctx)
{
    struct task_struct *task = (struct task_struct *)bpf_get_current_task();
    struct task_struct *parent = NULL;
    pid_t tgid = 0;

    bpf_probe_read_kernel(&parent, sizeof(parent), &task->real_parent);
    bpf_probe_read_kernel(&tgid, sizeof(tgid), &parent->tgid);
    bpf_printk("manual: pid = %d, ppid = %d\n", ctx->pid, tgid);
    return 0;
}

/* 二：BPF_CORE_READ 展开成同样两次读取 */
SEC("tp/sched/sched_process_exit")
int core(struct trace_event_raw_sched_process_template *ctx)
{
    struct task_struct *task = (struct task_struct *)bpf_get_current_task();

    bpf_printk("core: pid = %d, ppid = %d\n", ctx->pid,
               BPF_CORE_READ(task, real_parent, tgid));
    return 0;
}

/* 三：fentry 的参数与内核指针带类型，可以直接解引用 */
SEC("fentry/do_exit")
int BPF_PROG(direct, long code)
{
    struct task_struct *task = bpf_get_current_task_btf();

    bpf_printk("direct: ppid = %d\n", task->real_parent->tgid);
    return 0;
}

/* 四：从用户空间内存里读 connect() 的地址，留给习题补完。
 * 现在只读了 sa_family，端口与地址还得自己加。 */
SEC("tp/syscalls/sys_enter_connect")
int user_read(struct trace_event_raw_sys_enter *ctx)
{
    __u16 family = 0;

    BPF_CORE_READ_USER_INTO(&family, (struct sockaddr_in *)ctx->args[1], sin_family);
    bpf_printk("connect: family = %u\n", family);
    return 0;
}