/* 错误示例：符号名与 SEC 节名相同，生成 ELF 目标文件时 clang 报错。 */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char license[] SEC("license") = "Dual BSD/GPL";

SEC("tracepoint/syscalls/sys_enter_openat")
int count_openat(void *ctx) {
    (void)ctx;
    return 0;
}
