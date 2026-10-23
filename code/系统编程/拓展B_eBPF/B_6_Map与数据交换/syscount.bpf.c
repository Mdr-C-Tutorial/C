#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, char[TASK_COMM_LEN]);
    __type(value, __u64);
} counts SEC(".maps");

SEC("tp/raw_syscalls/sys_enter")
int count_syscalls(void *ctx)
{
    char key[TASK_COMM_LEN] = {};
    __u64 one = 1;
    __u64 *val;

    (void)ctx;
    bpf_get_current_comm(key, sizeof(key));
    val = bpf_map_lookup_elem(&counts, key);
    if (val != NULL) {
        __sync_fetch_and_add(val, 1);
        return 0;
    }

    /* 键还不存在：只在没有别人抢先建立时插入 */
    if (bpf_map_update_elem(&counts, key, &one, BPF_NOEXIST) == 0) {
        return 0;
    }

    /* 别的 CPU 刚建立了同一个键，回到它上面加 */
    val = bpf_map_lookup_elem(&counts, key);
    if (val != NULL) {
        __sync_fetch_and_add(val, 1);
    }
    return 0;
}
