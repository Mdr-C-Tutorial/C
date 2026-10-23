#include "vmlinux.h"

#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct record {
    __u32 pid;
    __u64 ts;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);
    __type(value, struct record);
} records SEC(".maps");

SEC("tp/sched/sched_process_exec")
int record_exec(void *ctx)
{
    struct record rec;

    (void)ctx;
    rec.pid = (__u32)(bpf_get_current_pid_tgid() >> 32);
    rec.ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&records, &rec.pid, &rec, BPF_ANY);
    return 0;
}