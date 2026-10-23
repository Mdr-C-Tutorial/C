// SPDX-License-Identifier: GPL-2.0
#include "vmlinux.h"
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 拒绝的目标端口，读的时候要按网络字节序比较 */
const volatile __u16 deny_port = 20002;

SEC("cgroup/connect4")
int connect_deny(struct bpf_sock_addr *ctx) {
    /* 只处理 TCP，其余交给后面的检查 */
    if (ctx->type != SOCK_STREAM) {
        return 1;
    }

    /* user_port 是网络字节序 */
    if (ctx->user_port == bpf_htons(deny_port)) {
        return 0;
    }

    return 1;
}
