/* 错误示例：节名拼错，libbpf 认不出程序类型。 */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tracepointX/syscalls/sys_enter_openat")
int count_openat(void *ctx) {
    (void)ctx;
    return 0;
}
