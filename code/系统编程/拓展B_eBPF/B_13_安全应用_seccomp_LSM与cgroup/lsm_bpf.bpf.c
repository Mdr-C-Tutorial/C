// SPDX-License-Identifier: GPL-2.0
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* vmlinux.h 不带 errno 的宏，用到哪个自己定义 */
#define EPERM 1

/* 挂在 bpf() 系统调用的 LSM 钩子上，拒绝一切 bpf() 调用 */
SEC("lsm/bpf")
int BPF_PROG(lsm_bpf, int cmd, union bpf_attr *attr, unsigned int size, int ret) {
    /* ret 是前一个 LSM 程序的结果，不是 0 就沿用它 */
    if (ret != 0) {
        return ret;
    }

    bpf_printk("LSM: block bpf() worked, cmd = %d", cmd);
    return -EPERM;
}
