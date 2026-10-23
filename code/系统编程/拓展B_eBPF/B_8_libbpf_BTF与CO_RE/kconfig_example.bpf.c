// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* 用 __kconfig 外部对象按内核版本选分支，与 task_state 的字段存在性检查对照。
 * 只用于演示 .kconfig 节与版本号重定位，加载后不打印有意义的业务数据。 */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

extern int LINUX_KERNEL_VERSION __kconfig;
extern bool CONFIG_BPF_JIT_ALWAYS_ON __kconfig __weak;

SEC("tp/sched/sched_process_exec")
int handle_exec(void *ctx)
{
    if (LINUX_KERNEL_VERSION >= KERNEL_VERSION(5, 14, 0)) {
        bpf_printk("__state\n");
    } else {
        bpf_printk("state\n");
    }
    if (CONFIG_BPF_JIT_ALWAYS_ON) {
        bpf_printk("jit always on\n");
    }
    return 0;
}
