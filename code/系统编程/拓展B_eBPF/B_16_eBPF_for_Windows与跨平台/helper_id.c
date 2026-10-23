/* 两套头文件下同一个辅助函数的编号不同。
 *
 * Linux:
 *   clang -target bpf -O2 -c helper_id.c -o helper_linux.o
 * Windows:
 *   clang -target bpf -O2 -DMDR_PLATFORM_WINDOWS -I<ebpf-for-windows>\include \
 *         -c helper_id.c -o helper_windows.o
 */
#if defined(MDR_PLATFORM_WINDOWS)
#include "bpf_helpers.h"
#else
typedef unsigned long long __u64;
typedef unsigned int __u32;
/* 编号取自 Linux 的 bpf/bpf_helper_defs.h */
static __u64 (*const bpf_ktime_get_ns)(void) = (void *)5;
static __u64 (*const bpf_get_current_pid_tgid)(void) = (void *)14;
#define SEC(NAME) __attribute__((section(NAME), used))
#endif

SEC("cgroup/connect4")
int probe_helpers(void *ctx) {
    __u64 t = bpf_ktime_get_ns();
    __u64 p = bpf_get_current_pid_tgid();

    (void)ctx;
    if (p == 0) {
        return (int)t;
    }
    return 0;
}
