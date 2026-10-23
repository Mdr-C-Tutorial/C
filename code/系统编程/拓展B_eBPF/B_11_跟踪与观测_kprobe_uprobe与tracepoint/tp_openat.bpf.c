#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 系统调用类事件共用 trace_event_raw_sys_enter：
 * id 是系统调用号，args[6] 是六个参数。 */
SEC("tp/syscalls/sys_enter_openat")
int handle_openat(struct trace_event_raw_sys_enter *ctx)
{
    char filename[64];
    const char *user_ptr = (const char *)ctx->args[1];
    pid_t pid = bpf_get_current_pid_tgid() >> 32;

    /* args[1] 指向用户空间内存，只能用辅助函数读 */
    if (bpf_probe_read_user_str(filename, sizeof(filename), user_ptr) < 0) {
        return 0;
    }
    bpf_printk("openat: pid = %d, filename = %s\n", pid, filename);
    return 0;
}