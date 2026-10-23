/* 按进程名统计系统调用次数，对应 bpftrace 的 @[comm] = count()。 */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, char[16]);
    __type(value, __u64);
} syscalls SEC(".maps");

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tracepoint/raw_syscalls/sys_enter")
int count_syscall(void *ctx) {
    char comm[16];
    __u64 one = 1;
    __u64 *n;

    (void)ctx;
    bpf_get_current_comm(comm, sizeof(comm));
    n = bpf_map_lookup_elem(&syscalls, comm);
    if (n != 0) {
        __sync_fetch_and_add(n, 1);
        return 0;
    }
    bpf_map_update_elem(&syscalls, comm, &one, BPF_ANY);
    return 0;
}
