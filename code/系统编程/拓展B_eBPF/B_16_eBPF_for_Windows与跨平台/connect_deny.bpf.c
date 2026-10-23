/* 一份源码，两个平台：拒绝连接到 127.0.0.1:20002。
 *
 * Linux:
 *   clang -g -O2 -target bpf -mcpu=v3 -I. -c connect_deny.bpf.c -o connect_deny.bpf.o
 * Windows（eBPF for Windows 的 include 目录）:
 *   clang -target bpf -Werror -O2 -DMDR_PLATFORM_WINDOWS -I<ebpf-for-windows>\include -I. \
 *         -c connect_deny.bpf.c -o connect_deny.bpf.o
 *
 * 两边的差别只在 ctx 的类型名和两个返回值的取值，判断逻辑一样。
 */
#if defined(MDR_PLATFORM_WINDOWS)
#include "bpf_endian.h"
#include "bpf_helpers.h"
typedef bpf_sock_addr_t connect_ctx_t;
#define VERDICT_ALLOW BPF_SOCK_ADDR_VERDICT_PROCEED_SOFT
#define VERDICT_DENY BPF_SOCK_ADDR_VERDICT_REJECT
#else
#include "vmlinux.h"
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
typedef struct bpf_sock_addr connect_ctx_t;
#define VERDICT_ALLOW 1
#define VERDICT_DENY 0
#endif

#include "connect_deny.h"

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} blocked SEC(".maps");

SEC("cgroup/connect4")
int connect4(connect_ctx_t *ctx) {
    if (bpf_ntohl(ctx->user_ip4) == DENY_ADDR && bpf_ntohs(ctx->user_port) == DENY_PORT) {
        __u32 key = 0;
        __u64 *count = bpf_map_lookup_elem(&blocked, &key);

        if (count != 0) {
            __sync_fetch_and_add(count, 1);
        }
        return VERDICT_DENY;
    }
    return VERDICT_ALLOW;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
