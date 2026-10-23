#include "vmlinux.h"

#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tp/sched/sched_process_exec")
int count_exec(void *ctx)
{
    char comm[16];

    (void)ctx;
    bpf_get_current_comm(comm, sizeof(comm));
    return 0;
}