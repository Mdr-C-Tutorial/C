/* 按进程名统计 openat 的调用次数，与 bpftrace 的 @[comm] = count() 等价。 */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, char[16]);
    __type(value, __u64);
} opens SEC(".maps");

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tracepoint/syscalls/sys_enter_openat")
int count_openat(void *ctx) {
    char comm[16];
    __u64 one = 1;
    __u64 *n;

    (void)ctx;
    bpf_get_current_comm(comm, sizeof(comm));
    n = bpf_map_lookup_elem(&opens, comm);
    if (n != 0) {
        __sync_fetch_and_add(n, 1);
        return 0;
    }
    bpf_map_update_elem(&opens, comm, &one, BPF_ANY);
    return 0;
}
