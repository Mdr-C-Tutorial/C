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
int drain_count(void *ctx)
{
    __u32 key = 0;
    __u64 acc = 1;
    __u64 i;
    __u64 *value;

    (void)ctx;
    value = bpf_map_lookup_elem(&counts, &key);
    if (value == NULL) {
        return 0;
    }
    for (i = 0; i < *value; i++) {
        acc = acc * 3 + 1;
    }
    *value = acc;
    return 0;
}