#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tp/syscalls/sys_enter_getpid")
int handle_getpid(void *ctx) {
    (void)ctx;
    return 0;
}
