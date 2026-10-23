#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

SEC("tp/syscalls/sys_enter_write")
int big_stack(void *ctx)
{
    char buf[1024];
    int i;

    for (i = 0; i < 1024; i++) {
        buf[i] = i;
    }
    bpf_printk("last byte %d\n", buf[1023]);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
