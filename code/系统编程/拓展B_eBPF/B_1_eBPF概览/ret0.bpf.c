/* ret0.bpf.c：本页用来观察 eBPF 指令编码的最小程序。 */
#define SEC(name) __attribute__((section(name), used))

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tracepoint/syscalls/sys_enter_openat")
int ret0(void *ctx) {
    (void)ctx;
    return 0;
}
