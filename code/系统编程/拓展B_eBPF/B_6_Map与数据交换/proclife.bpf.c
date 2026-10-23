#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>

#include "proclife.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

const volatile __u64 min_duration_ns = 0;

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, pid_t);
    __type(value, __u64);
} exec_start SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

SEC("tp/sched/sched_process_exec")
int handle_exec(struct trace_event_raw_sched_process_exec *ctx)
{
    struct task_struct *task;
    struct event *e;
    unsigned int fname_off;
    pid_t pid;
    __u64 ts;

    pid = bpf_get_current_pid_tgid() >> 32;
    ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&exec_start, &pid, &ts, BPF_ANY);

    if (min_duration_ns != 0) {
        return 0;
    }

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (e == NULL) {
        return 0;
    }

    task = (struct task_struct *)bpf_get_current_task();
    e->pid = pid;
    e->ppid = BPF_CORE_READ(task, real_parent, tgid);
    e->exit_code = 0;
    e->duration_ns = 0;
    e->exit_event = 0;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    fname_off = ctx->__data_loc_filename & 0xFFFF;
    bpf_probe_read_kernel_str(e->filename, sizeof(e->filename),
                              (char *)ctx + fname_off);

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tp/sched/sched_process_exit")
int handle_exit(struct trace_event_raw_sched_process_template *ctx)
{
    struct task_struct *task;
    struct event *e;
    pid_t pid;
    pid_t tid;
    __u64 *start_ts;
    __u64 id;
    __u64 duration_ns = 0;

    (void)ctx;
    id = bpf_get_current_pid_tgid();
    pid = id >> 32;
    tid = (__u32)id;

    /* 线程退出时 pid 与 tid 不同，只看进程的退出 */
    if (pid != tid) {
        return 0;
    }

    start_ts = bpf_map_lookup_elem(&exec_start, &pid);
    if (start_ts != NULL) {
        duration_ns = bpf_ktime_get_ns() - *start_ts;
        bpf_map_delete_elem(&exec_start, &pid);
    } else if (min_duration_ns != 0) {
        /* 没有记录到 exec，算不出存活时间，指定了最短时长时就不报告 */
        return 0;
    }

    if (min_duration_ns != 0 && duration_ns < min_duration_ns) {
        return 0;
    }

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (e == NULL) {
        return 0;
    }

    task = (struct task_struct *)bpf_get_current_task();
    e->pid = pid;
    e->ppid = BPF_CORE_READ(task, real_parent, tgid);
    e->exit_code = (BPF_CORE_READ(task, exit_code) >> 8) & 0xFF;
    e->duration_ns = duration_ns;
    e->exit_event = 1;
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    e->filename[0] = '\0';

    bpf_ringbuf_submit(e, 0);
    return 0;
}