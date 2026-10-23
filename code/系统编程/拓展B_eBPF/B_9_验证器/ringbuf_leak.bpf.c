#include "vmlinux.h"

#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct event {
    __u32 pid;
    __u32 _pad;
    char comm[16];
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

SEC("tp/sched/sched_process_exec")
int report_exec(void *ctx)
{
    struct event *e;
    __u32 pid;

    (void)ctx;
    pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (e == NULL) {
        return 0;
    }
    e->pid = pid;
    e->comm[0] = 'x';
    if (pid == 1) {
        return 0;
    }
    bpf_ringbuf_submit(e, 0);
    return 0;
}