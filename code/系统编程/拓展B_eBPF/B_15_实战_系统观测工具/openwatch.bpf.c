#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

#include "openwatch.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

const volatile __u32 targ_tgid = 0;
const volatile bool targ_failed = false;

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, __u32);
    __type(value, struct open_args);
} start SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} dropped SEC(".maps");

static __always_inline void count_dropped(void) {
    __u32 zero = 0;
    __u64 *cnt = bpf_map_lookup_elem(&dropped, &zero);

    if (cnt != NULL) {
        (*cnt) += 1;
    }
}

SEC("tracepoint/syscalls/sys_enter_openat")
int handle_enter(struct trace_event_raw_sys_enter *ctx) {
    __u64 id = bpf_get_current_pid_tgid();
    __u32 tid = (__u32)id;
    struct open_args args = {};

    if (targ_tgid != 0 && targ_tgid != (__u32)(id >> 32)) {
        return 0;
    }

    args.filename = (const char *)ctx->args[1];
    args.flags = ctx->args[2];
    bpf_map_update_elem(&start, &tid, &args, BPF_ANY);
    return 0;
}

SEC("tracepoint/syscalls/sys_exit_openat")
int handle_exit(struct trace_event_raw_sys_exit *ctx) {
    __u64 id = bpf_get_current_pid_tgid();
    __u32 tid = (__u32)id;
    struct open_args *args;
    struct event *e;

    args = bpf_map_lookup_elem(&start, &tid);
    if (args == NULL) {
        return 0;
    }

    if (targ_failed && ctx->ret >= 0) {
        goto cleanup;
    }

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (e == NULL) {
        count_dropped();
        goto cleanup;
    }

    e->ts = bpf_ktime_get_ns();
    e->pid = (__u32)(id >> 32);
    e->tid = tid;
    e->uid = (__u32)bpf_get_current_uid_gid();
    e->ret = (__s32)ctx->ret;
    e->flags = args->flags;
    bpf_get_current_comm(&e->comm, sizeof(e->comm));
    if (bpf_probe_read_user_str(&e->path, sizeof(e->path), args->filename) < 0) {
        e->path[0] = '\0';
    }
    bpf_ringbuf_submit(e, 0);

cleanup:
    bpf_map_delete_elem(&start, &tid);
    return 0;
}
