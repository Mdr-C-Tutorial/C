#include "vmlinux.h"

#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);
    __type(value, __u64);
} counts SEC(".maps");

SEC("tp/raw_syscalls/sys_enter")
int count_syscall(void *ctx)
{
    __u32 key;
    __u64 *value;

    (void)ctx;
    key = (__u32)bpf_get_current_pid_tgid();
    value = bpf_map_lookup_elem(&counts, &key);
    if (value == NULL) {
        return 0;
    }
    *value += 1;
    return 0;
}