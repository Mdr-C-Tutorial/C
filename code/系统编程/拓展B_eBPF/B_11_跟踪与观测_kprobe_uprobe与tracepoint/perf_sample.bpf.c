#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "perf_sample.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

/* 每次采样触发一次：取当前进程、CPU、命令名和内核调用栈，
 * 装进样本后送进环形缓冲区。 */
SEC("perf_event")
int profile(void *ctx)
{
    struct stack_sample *e;

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e) {
        return 0;
    }
    e->pid = bpf_get_current_pid_tgid() >> 32;
    e->cpu = bpf_get_smp_processor_id();
    bpf_get_current_comm(e->comm, sizeof(e->comm));
    e->kstack_sz = bpf_get_stack(ctx, e->kstack, sizeof(e->kstack), 0);
    bpf_ringbuf_submit(e, 0);
    return 0;
}